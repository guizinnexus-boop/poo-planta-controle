// Testa o Controlador de ponta a ponta com uma planta simulada em memoria (arquivos temporarios).
#include <fstream>

#include "../../controlador/controlador.hpp"
#include "../../controlador/tempo.hpp"
#include "teste_util.hpp"

using namespace poo;

static const long long T0 = 1767261600;  // 2026-01-01T10:00:00Z
static const std::string TS0 = "2026-01-01T10:00:00Z";

// Escreve planta.json como a planta escreveria (valores escolhidos, sem aleatoriedade).
static void planta(const RepositorioArquivos& repo, const std::string& sessao, long long ciclo, const std::string& ts,
                   const std::string& leit_nivel, const std::string& leit_temp, long long ultimo = 0,
                   const std::string& erro = "null", const std::string& bomba = "0.0", const std::string& aquec = "0.0") {
    std::string t = "{\"versao\":1,\"sessao\":\"" + sessao + "\",\"ciclo\":" + std::to_string(ciclo) + ",\"timestamp\":\"" + ts + "\"," +
                    "\"leituras\":[" + leit_nivel + "," + leit_temp + "],\"atuadores\":{\"bomba\":" + bomba + ",\"aquecedor\":" + aquec + "}," +
                    "\"ultimo_comando\":" + std::to_string(ultimo) + ",\"erro_comando\":" + erro + ",\"motivo_seguro\":null}";
    repo.gravar(ARQ_PLANTA, Json::analisar(t));
}
static std::string LT(const std::string& v, const std::string& st = "operando") {
    return "{\"tag\":\"LT-01\",\"tipo\":\"nivel\",\"valor\":" + v + ",\"unidade\":\"%\",\"status\":\"" + st + "\"}";
}
static std::string TT(const std::string& v, const std::string& st = "operando") {
    return "{\"tag\":\"TT-01\",\"tipo\":\"temperatura\",\"valor\":" + v + ",\"unidade\":\"C\",\"status\":\"" + st + "\"}";
}
static Controlador criar(const std::string& dir, const std::string& politica, long long limite = 10) {
    ConfigControlador cfg; cfg.limite_obsoleto_s = limite;
    return Controlador(RepositorioArquivos(dir), criar_politica(politica), cfg);
}
static double atuador(const Json& cmd, const char* nome) { return cmd.buscar("atuadores")->buscar(nome)->numero_valor(); }
static bool tem_alarme(const Json& tele, const std::string& cod) {
    for (const auto& a : tele.buscar("alarmes")->itens()) if (a.buscar("codigo")->texto_valor() == cod) return true;
    return false;
}

static void ciclo_completo_e_confirmacao() {
    DiretorioTemporario tmp; RepositorioArquivos repo(tmp.caminho());
    auto ctl = criar(tmp.caminho(), "proporcional");
    planta(repo, "s1", 1, TS0, LT("55"), TT("38"));
    auto r = ctl.executar(T0 + 1);
    VERIFICAR(r.situacao == SituacaoCiclo::SessaoReiniciada && r.comando_emitido && r.id_comando == 1, "primeiro ciclo: comando id 1");
    Json cmd = repo.ler(ARQ_COMANDO).documento;
    VERIFICAR_IGUAL(cmd.buscar("sessao")->texto_valor(), std::string("s1"), "comando na sessao da planta");
    VERIFICAR_PROXIMO(atuador(cmd, "bomba"), 25.0, "politica proporcional: 5*(60-55)");
    VERIFICAR_PROXIMO(atuador(cmd, "aquecedor"), 20.0, "politica proporcional: 10*(40-38)");

    Json tele = repo.ler(ARQ_TELEMETRIA).documento;
    VERIFICAR_IGUAL(tele.buscar("ciclo")->inteiro_valor(), 1LL, "telemetria preserva o ciclo de origem");
    VERIFICAR_IGUAL(tele.buscar("leituras")->itens().size(), size_t(2), "telemetria preserva leituras");
    VERIFICAR_IGUAL(tele.buscar("controlador")->buscar("confirmacao")->buscar("estado")->texto_valor(), std::string("nenhum"), "ainda sem confirmacao");
    VERIFICAR_PROXIMO(tele.buscar("atuadores")->buscar("bomba")->numero_valor(), 0.0, "atuacao OBSERVADA continua a da planta");
    VERIFICAR_IGUAL(tele.buscar("controlador")->buscar("solicitacao")->buscar("id")->inteiro_valor(), 1LL, "solicitacao em campo separado, com id");

    // A planta aceita o id 1 e o reflete no snapshot seguinte.
    planta(repo, "s1", 2, TS0, LT("56"), TT("38.5"), 1, "null", "25.0", "20.0");
    r = ctl.executar(T0 + 2);
    VERIFICAR(r.situacao == SituacaoCiclo::NovaAquisicao && r.id_comando == 2, "segundo ciclo: id 2");
    VERIFICAR_IGUAL(r.confirmacao, std::string("confirmado"), "id 1 confirmado pela planta");
    tele = repo.ler(ARQ_TELEMETRIA).documento;
    VERIFICAR_PROXIMO(tele.buscar("atuadores")->buscar("bomba")->numero_valor(), 25.0, "atuacao observada = aplicada pela planta");
    VERIFICAR_IGUAL(tele.buscar("controlador")->buscar("confirmacao")->buscar("id")->inteiro_valor(), 1LL, "confirmacao refere o id 1");

    // Planta nao acompanha (ultimo_comando fica em 1): id 2 permanece pendente.
    planta(repo, "s1", 3, TS0, LT("57"), TT("39"), 1);
    r = ctl.executar(T0 + 3);
    VERIFICAR_IGUAL(r.confirmacao, std::string("pendente"), "id 2 nao confirmado => pendente");
    VERIFICAR_IGUAL(r.id_comando, 3LL, "ids seguem crescendo");
    // Planta rejeita com erro_comando.
    planta(repo, "s1", 4, TS0, LT("57"), TT("39"), 1, "\"comando_invalido\"");
    r = ctl.executar(T0 + 4);
    VERIFICAR_IGUAL(r.confirmacao, std::string("rejeitado"), "erro_comando => rejeitado");
    VERIFICAR(r.id_comando > 1, "id novo maior que o ultimo confirmado");
}

static void nao_renova_ao_reler_ciclo() {
    DiretorioTemporario tmp; RepositorioArquivos repo(tmp.caminho());
    auto ctl = criar(tmp.caminho(), "liga-desliga");
    planta(repo, "s1", 5, TS0, LT("10"), TT("10"));
    auto r1 = ctl.executar(T0);
    VERIFICAR(r1.comando_emitido, "primeira leitura comanda");
    std::string cmd1 = repo.ler(ARQ_COMANDO).documento.serializar();
    auto r2 = ctl.executar(T0 + 1);
    VERIFICAR(r2.situacao == SituacaoCiclo::CicloRepetido && !r2.comando_emitido, "mesmo ciclo: nao emite");
    VERIFICAR_IGUAL(repo.ler(ARQ_COMANDO).documento.serializar(), cmd1, "arquivo de comando intacto ao reler");
    VERIFICAR(r2.telemetria_gravada, "telemetria continua sendo atualizada");
    // Reiniciar o processo (novo objeto) tambem nao duplica: estado persistido.
    auto outro = criar(tmp.caminho(), "liga-desliga");
    auto r3 = outro.executar(T0 + 2);
    VERIFICAR(r3.situacao == SituacaoCiclo::CicloRepetido && !r3.comando_emitido, "reinicio do controlador nao reemite");
    VERIFICAR_IGUAL(repo.ler(ARQ_COMANDO).documento.serializar(), cmd1, "comando ainda intacto apos reinicio");
    // Ciclo menor que o processado e ignorado.
    planta(repo, "s1", 4, TS0, LT("10"), TT("10"));
    auto r4 = ctl.executar(T0 + 3);
    VERIFICAR(r4.situacao == SituacaoCiclo::CicloAnterior && !r4.comando_emitido && !r4.entrada_ok(), "ciclo anterior ignorado");
    VERIFICAR_IGUAL(repo.ler(ARQ_COMANDO).documento.serializar(), cmd1, "comando intacto apos ciclo anterior");
}

static void duas_politicas_decisoes_diferentes() {
    DiretorioTemporario a, b;
    RepositorioArquivos ra(a.caminho()), rb(b.caminho());
    planta(ra, "s", 1, TS0, LT("55"), TT("38"));
    planta(rb, "s", 1, TS0, LT("55"), TT("38"));
    criar(a.caminho(), "liga-desliga").executar(T0);
    criar(b.caminho(), "proporcional").executar(T0);
    double onoff = atuador(ra.ler(ARQ_COMANDO).documento, "bomba");
    double prop = atuador(rb.ler(ARQ_COMANDO).documento, "bomba");
    VERIFICAR_PROXIMO(onoff, 100.0, "liga-desliga => 100");
    VERIFICAR_PROXIMO(prop, 25.0, "proporcional => 25");
    VERIFICAR_IGUAL(ra.ler(ARQ_TELEMETRIA).documento.buscar("controlador")->buscar("politica")->texto_valor(), std::string("liga-desliga"), "telemetria registra a politica");
}

static void reinicio_de_sessao() {
    DiretorioTemporario tmp; RepositorioArquivos repo(tmp.caminho());
    auto ctl = criar(tmp.caminho(), "proporcional");
    planta(repo, "s1", 9, TS0, LT("50"), TT("30"));
    ctl.executar(T0);
    planta(repo, "s1", 10, TS0, LT("50"), TT("30"), 1);
    auto r = ctl.executar(T0 + 1);
    VERIFICAR(r.id_comando == 2, "sessao s1 chegou ao id 2");
    // Planta reiniciou: sessao nova, ciclo volta a 1, ultimo_comando volta a 0.
    planta(repo, "s2", 1, TS0, LT("50"), TT("30"), 0);
    r = ctl.executar(T0 + 2);
    VERIFICAR(r.situacao == SituacaoCiclo::SessaoReiniciada && r.comando_emitido, "nova sessao comanda");
    VERIFICAR_IGUAL(r.id_comando, 1LL, "ids reiniciam na nova sessao (ultimo_comando=0)");
    VERIFICAR_IGUAL(r.confirmacao, std::string("nenhum"), "nao herda confirmacao da sessao anterior");
    VERIFICAR_IGUAL(repo.ler(ARQ_COMANDO).documento.buscar("sessao")->texto_valor(), std::string("s2"), "comando usa a nova sessao");
    // E o ciclo 1 de s2 relido nao comanda de novo.
    VERIFICAR(!ctl.executar(T0 + 3).comando_emitido, "ciclo 1 de s2 relido nao reemite");
}

static void falhas_de_entrada_preservam_telemetria() {
    DiretorioTemporario tmp; RepositorioArquivos repo(tmp.caminho());
    auto ctl = criar(tmp.caminho(), "proporcional");
    auto r = ctl.executar(T0);
    VERIFICAR(r.situacao == SituacaoCiclo::Indisponivel && !r.entrada_ok() && !r.diagnosticos.empty(), "arquivo ausente => diagnostico");
    VERIFICAR(repo.ler(ARQ_TELEMETRIA).status == RepositorioArquivos::Status::Ausente && repo.ler(ARQ_COMANDO).status == RepositorioArquivos::Status::Ausente, "nada e inventado");

    planta(repo, "s1", 1, TS0, LT("55"), TT("38"));
    ctl.executar(T0);
    std::string tele_boa = repo.ler(ARQ_TELEMETRIA).documento.serializar();
    std::string cmd_bom = repo.ler(ARQ_COMANDO).documento.serializar();

    { std::ofstream(repo.caminho(ARQ_PLANTA)) << "{\"versao\": 1, \"sess"; }  // truncado
    r = ctl.executar(T0 + 1);
    VERIFICAR(r.situacao == SituacaoCiclo::Indisponivel && !r.entrada_ok(), "JSON invalido => indisponivel");
    VERIFICAR_IGUAL(repo.ler(ARQ_TELEMETRIA).documento.serializar(), tele_boa, "telemetria valida anterior preservada");
    VERIFICAR_IGUAL(repo.ler(ARQ_COMANDO).documento.serializar(), cmd_bom, "nenhum comando novo com entrada invalida");

    { std::ofstream(repo.caminho(ARQ_PLANTA)) << R"({"versao":3,"sessao":"s1","ciclo":2})"; }
    r = ctl.executar(T0 + 2);
    VERIFICAR(r.situacao == SituacaoCiclo::Invalida && r.diagnosticos.size() >= 2, "violacao de contrato => invalida com diagnosticos");
    VERIFICAR_IGUAL(repo.ler(ARQ_TELEMETRIA).documento.serializar(), tele_boa, "telemetria preservada tambem aqui");

    // Recuperacao: arquivo volta valido e o controlador retoma sem reaproveitar nada antigo.
    planta(repo, "s1", 2, TS0, LT("56"), TT("39"), 1);
    r = ctl.executar(T0 + 3);
    VERIFICAR(r.entrada_ok() && r.comando_emitido && r.id_comando == 2, "recupera e comanda com id novo");
}

static void falha_de_sensor_e_recuperacao() {
    DiretorioTemporario tmp; RepositorioArquivos repo(tmp.caminho());
    auto ctl = criar(tmp.caminho(), "proporcional");
    // TT-01 em falha: aquecedor 0, nivel continua controlado, alarme especifico.
    planta(repo, "s1", 1, TS0, LT("50"), TT("null", "falha"));
    ctl.executar(T0);
    Json cmd = repo.ler(ARQ_COMANDO).documento;
    VERIFICAR_PROXIMO(atuador(cmd, "aquecedor"), 0.0, "TT-01 em falha => aquecedor em 0");
    VERIFICAR_PROXIMO(atuador(cmd, "bomba"), 50.0, "LT-01 valido => bomba 5*(60-50)");
    Json tele = repo.ler(ARQ_TELEMETRIA).documento;
    VERIFICAR(tem_alarme(tele, "FALHA_SENSOR_TT_01"), "alarme de falha do TT-01");
    VERIFICAR(!tem_alarme(tele, "AQUISICAO_OBSOLETA"), "falha de sensor nao e obsolescencia");
    VERIFICAR(tele.buscar("leituras")->itens()[1].buscar("valor")->eh_nulo(), "telemetria nao inventa temperatura");
    // LT-01 em falha: bomba 0, nao usa nivel antigo.
    planta(repo, "s1", 2, TS0, LT("null", "falha"), TT("30"), 1);
    ctl.executar(T0 + 1);
    cmd = repo.ler(ARQ_COMANDO).documento;
    VERIFICAR_PROXIMO(atuador(cmd, "bomba"), 0.0, "LT-01 em falha => bomba 0");
    VERIFICAR_PROXIMO(atuador(cmd, "aquecedor"), 100.0, "TT-01 valido => aquecedor 10*(40-30) satura");
    // Recuperacao: nova aquisicao valida remove o alarme e retoma o controle.
    planta(repo, "s1", 3, TS0, LT("50"), TT("30"), 2);
    ctl.executar(T0 + 2);
    tele = repo.ler(ARQ_TELEMETRIA).documento;
    VERIFICAR(!tem_alarme(tele, "FALHA_SENSOR_LT_01") && !tem_alarme(tele, "FALHA_SENSOR_TT_01"), "alarmes de sensor somem com leitura valida nova");
    VERIFICAR_PROXIMO(atuador(repo.ler(ARQ_COMANDO).documento, "bomba"), 50.0, "controle de nivel retomado");
}

static void obsolescencia() {
    DiretorioTemporario tmp; RepositorioArquivos repo(tmp.caminho());
    auto ctl = criar(tmp.caminho(), "liga-desliga", 10);
    planta(repo, "s1", 1, TS0, LT("10"), TT("10"));
    // Idade exatamente igual ao limite: ainda e valida (fronteira).
    ctl.executar(T0 + 10);
    VERIFICAR_PROXIMO(atuador(repo.ler(ARQ_COMANDO).documento, "bomba"), 100.0, "idade == limite usa a politica");
    VERIFICAR(!tem_alarme(repo.ler(ARQ_TELEMETRIA).documento, "AQUISICAO_OBSOLETA"), "idade == limite nao e obsoleta");
    // Mesmo ciclo reavaliado depois do limite: alarme aparece, sem novo comando.
    auto r = ctl.executar(T0 + 11);
    VERIFICAR(!r.comando_emitido, "ciclo repetido nao comanda mesmo obsoleto");
    VERIFICAR(tem_alarme(repo.ler(ARQ_TELEMETRIA).documento, "AQUISICAO_OBSOLETA"), "idade > limite => alarme de obsolescencia");
    VERIFICAR(repo.ler(ARQ_TELEMETRIA).documento.buscar("controlador")->buscar("aquisicao_obsoleta")->booleano_valor(), "flag de obsolescencia na telemetria");
    // Ciclo novo, mas com timestamp antigo: estado seguro, politica nao usada.
    planta(repo, "s1", 2, TS0, LT("10"), TT("10"), 1);
    r = ctl.executar(T0 + 500);
    VERIFICAR(r.comando_emitido, "ciclo novo comanda");
    Json cmd = repo.ler(ARQ_COMANDO).documento;
    VERIFICAR(atuador(cmd, "bomba") == 0.0 && atuador(cmd, "aquecedor") == 0.0, "aquisicao obsoleta => estado seguro 0/0");
    VERIFICAR_IGUAL(repo.ler(ARQ_TELEMETRIA).documento.buscar("controlador")->buscar("solicitacao")->buscar("motivo")->texto_valor(), std::string("aquisicao_obsoleta"), "motivo registrado");
}

static void alarmes_de_limite_na_telemetria() {
    DiretorioTemporario tmp; RepositorioArquivos repo(tmp.caminho());
    auto ctl = criar(tmp.caminho(), "proporcional");
    planta(repo, "s1", 1, TS0, LT("20"), TT("80"));
    ctl.executar(T0);
    Json tele = repo.ler(ARQ_TELEMETRIA).documento;
    VERIFICAR(tele.buscar("alarmes")->itens().empty(), "nivel 20 e temperatura 80 (fronteira) nao alarmam");
    planta(repo, "s1", 2, TS0, LT("19.9"), TT("80.1"), 1);
    ctl.executar(T0);
    tele = repo.ler(ARQ_TELEMETRIA).documento;
    VERIFICAR(tem_alarme(tele, "NIVEL_BAIXO") && tem_alarme(tele, "TEMPERATURA_ALTA"), "alem da fronteira os dois alarmes ativam");
    for (const auto& a : tele.buscar("alarmes")->itens())
        VERIFICAR(a.buscar("codigo")->tipo() == Json::Tipo::Texto && a.buscar("mensagem")->tipo() == Json::Tipo::Texto && a.buscar("severidade")->tipo() == Json::Tipo::Texto, "alarme com tres campos textuais");
}

static void estado_do_controlador_invalido_recupera() {
    DiretorioTemporario tmp; RepositorioArquivos repo(tmp.caminho());
    auto ctl = criar(tmp.caminho(), "proporcional");
    planta(repo, "s1", 1, TS0, LT("50"), TT("30"));
    ctl.executar(T0);
    { std::ofstream(repo.caminho(ARQ_ESTADO)) << "lixo"; }
    planta(repo, "s1", 2, TS0, LT("50"), TT("30"), 1);
    auto r = ctl.executar(T0 + 1);
    VERIFICAR(r.entrada_ok() && r.comando_emitido, "estado corrompido nao impede o controle");
    bool avisou = false;
    for (const auto& d : r.diagnosticos) if (d.find("ESTADO_CONTROLADOR_INVALIDO") != std::string::npos) avisou = true;
    VERIFICAR(avisou, "estado corrompido gera diagnostico");
    VERIFICAR(r.id_comando > 1, "id continua maior que o ultimo_comando da planta");
}

int main() {
    ciclo_completo_e_confirmacao();
    nao_renova_ao_reler_ciclo();
    duas_politicas_decisoes_diferentes();
    reinicio_de_sessao();
    falhas_de_entrada_preservam_telemetria();
    falha_de_sensor_e_recuperacao();
    obsolescencia();
    alarmes_de_limite_na_telemetria();
    estado_do_controlador_invalido_recupera();
    std::cout << "OK: Controlador (ciclo, confirmacao, repeticao, sessao, falhas, sensores, obsolescencia, alarmes)\n";
    return 0;
}

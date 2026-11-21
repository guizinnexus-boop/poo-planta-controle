// Testa as duas politicas (Strategy), ComandoAtuacao e as regras de alarme.
#include "../../controlador/alarmes.hpp"
#include "../../controlador/comando.hpp"
#include "../../controlador/politica.hpp"
#include "teste_util.hpp"

using namespace poo;

static void politicas_decisoes_diferentes() {
    PoliticaLigaDesliga onoff;      // alvo 60 / 40, ganhos 5 / 10
    PoliticaProporcional prop;
    EntradaControle e{55.0, 38.0};  // abaixo dos dois alvos
    Decisao a = onoff.decidir(e);
    Decisao b = prop.decidir(e);
    VERIFICAR_PROXIMO(a.bomba, 100.0, "liga-desliga: bomba 100 abaixo do alvo");
    VERIFICAR_PROXIMO(a.aquecedor, 100.0, "liga-desliga: aquecedor 100 abaixo do alvo");
    VERIFICAR_PROXIMO(b.bomba, 25.0, "proporcional: 5*(60-55)=25");
    VERIFICAR_PROXIMO(b.aquecedor, 20.0, "proporcional: 10*(40-38)=20");
    VERIFICAR(a.bomba != b.bomba && a.aquecedor != b.aquecedor, "as politicas decidem de forma diferente");
    VERIFICAR(onoff.nome() != prop.nome(), "nomes distintos");
}

static void politicas_fronteiras() {
    PoliticaLigaDesliga onoff;
    PoliticaProporcional prop;
    // Exatamente no alvo: liga-desliga desliga; proporcional da 0.
    VERIFICAR_PROXIMO(onoff.decidir({60.0, 40.0}).bomba, 0.0, "nivel == alvo => bomba 0");
    VERIFICAR_PROXIMO(onoff.decidir({60.0, 40.0}).aquecedor, 0.0, "temp == alvo => aquecedor 0");
    VERIFICAR_PROXIMO(onoff.decidir({59.999, 39.999}).bomba, 100.0, "logo abaixo do alvo => liga");
    VERIFICAR_PROXIMO(prop.decidir({60.0, 40.0}).bomba, 0.0, "proporcional no alvo => 0");
    // Saturacao em [0,100].
    VERIFICAR_PROXIMO(prop.decidir({0.0, 0.0}).bomba, 100.0, "erro 60*5=300 satura em 100");
    VERIFICAR_PROXIMO(prop.decidir({100.0, 100.0}).bomba, 0.0, "acima do alvo satura em 0");
    VERIFICAR_PROXIMO(prop.decidir({40.0, 0.0}).bomba, 100.0, "erro 20*5=100 exato");
    // Parametros customizados alteram a decisao.
    ParametrosControle p; p.alvo_nivel = 80.0;
    VERIFICAR_PROXIMO(PoliticaLigaDesliga(p).decidir({70.0, 99.0}).bomba, 100.0, "alvo 80: nivel 70 liga");
}

static void politicas_falha_e_estado_seguro() {
    PoliticaLigaDesliga onoff;
    PoliticaProporcional prop;
    EntradaControle sem_temp{30.0, std::nullopt};
    EntradaControle sem_nivel{std::nullopt, 10.0};
    for (const PoliticaControle* p : {static_cast<const PoliticaControle*>(&onoff), static_cast<const PoliticaControle*>(&prop)}) {
        VERIFICAR_PROXIMO(p->decidir(sem_temp).aquecedor, 0.0, p->nome() + ": sem TT-01 => aquecedor 0");
        VERIFICAR(p->decidir(sem_temp).bomba > 0.0, p->nome() + ": sem TT-01 o nivel segue controlado");
        VERIFICAR_PROXIMO(p->decidir(sem_nivel).bomba, 0.0, p->nome() + ": sem LT-01 => bomba 0 (nao usa nivel inventado)");
        VERIFICAR(p->decidir(sem_nivel).aquecedor > 0.0, p->nome() + ": sem LT-01 a temperatura segue controlada");
        Decisao nenhuma = p->decidir({std::nullopt, std::nullopt});
        VERIFICAR(nenhuma.bomba == 0.0 && nenhuma.aquecedor == 0.0, p->nome() + ": sem leituras => tudo 0");
    }
    ParametrosControle ruim; ruim.alvo_nivel = 101.0;
    VERIFICAR_LANCA(PoliticaLigaDesliga{ruim}, std::invalid_argument, "alvo > 100 rejeitado");
    ParametrosControle ganho0; ganho0.ganho_nivel = 0.0;
    VERIFICAR_LANCA(PoliticaProporcional{ganho0}, std::invalid_argument, "ganho zero rejeitado");
    VERIFICAR(criar_politica("liga-desliga") != nullptr && criar_politica("proporcional") != nullptr, "fabrica conhece as duas");
    VERIFICAR(criar_politica("inexistente") == nullptr, "fabrica rejeita nome desconhecido");
}

static void comando() {
    ComandoAtuacao c("s1", 3, 75.0, 20.0);
    Json j = c.para_json();
    VERIFICAR_IGUAL(j.buscar("versao")->inteiro_valor(), 1LL, "versao 1");
    VERIFICAR_IGUAL(j.buscar("sessao")->texto_valor(), std::string("s1"), "sessao");
    VERIFICAR_IGUAL(j.buscar("id")->inteiro_valor(), 3LL, "id");
    VERIFICAR_PROXIMO(j.buscar("atuadores")->buscar("bomba")->numero_valor(), 75.0, "bomba");
    VERIFICAR_PROXIMO(j.buscar("atuadores")->buscar("aquecedor")->numero_valor(), 20.0, "aquecedor");
    VERIFICAR_IGUAL(j.buscar("atuadores")->campos().size(), size_t(2), "exatamente bomba e aquecedor");
    // Fronteiras.
    ComandoAtuacao zero("s", 1, 0.0, 0.0);
    ComandoAtuacao cheio("s", 1, 100.0, 100.0);
    VERIFICAR_PROXIMO(cheio.bomba(), 100.0, "100 aceito");
    // Falhas.
    VERIFICAR_LANCA(ComandoAtuacao("s", 1, 100.1, 0), std::invalid_argument, "bomba > 100");
    VERIFICAR_LANCA(ComandoAtuacao("s", 1, 0, -0.1), std::invalid_argument, "aquecedor < 0");
    VERIFICAR_LANCA(ComandoAtuacao("s", 0, 0, 0), std::invalid_argument, "id 0");
    VERIFICAR_LANCA(ComandoAtuacao("", 1, 0, 0), std::invalid_argument, "sessao vazia");
    VERIFICAR_LANCA(ComandoAtuacao("s", 1, NAN, 0), std::invalid_argument, "NaN");
}

static Aquisicao aq(std::optional<double> n, std::optional<double> t) {
    Aquisicao a; a.nivel = n; a.temperatura = t; return a;
}

static bool tem(const std::vector<Alarme>& v, const std::string& cod) {
    for (const auto& a : v) if (a.codigo == cod) return true;
    return false;
}

static void alarmes() {
    LimitesAlarme lim;  // nivel < 20 ; temperatura > 80
    VERIFICAR(avaliar_alarmes(aq(50, 50), false, lim).empty(), "condicao normal sem alarmes");
    VERIFICAR(tem(avaliar_alarmes(aq(10, 50), false, lim), "NIVEL_BAIXO"), "nivel 10 alarma");
    VERIFICAR(tem(avaliar_alarmes(aq(50, 95), false, lim), "TEMPERATURA_ALTA"), "temp 95 alarma");
    // Fronteiras: igual ao limite NAO alarma; o menor passo alem alarma.
    VERIFICAR(!tem(avaliar_alarmes(aq(20.0, 50), false, lim), "NIVEL_BAIXO"), "nivel == 20 nao alarma");
    VERIFICAR(tem(avaliar_alarmes(aq(19.999, 50), false, lim), "NIVEL_BAIXO"), "nivel 19.999 alarma");
    VERIFICAR(!tem(avaliar_alarmes(aq(50, 80.0), false, lim), "TEMPERATURA_ALTA"), "temp == 80 nao alarma");
    VERIFICAR(tem(avaliar_alarmes(aq(50, 80.001), false, lim), "TEMPERATURA_ALTA"), "temp 80.001 alarma");
    // Obsolescencia e falha de sensor sao alarmes distintos.
    auto obs = avaliar_alarmes(aq(50, 50), true, lim);
    VERIFICAR(tem(obs, "AQUISICAO_OBSOLETA") && obs.size() == 1, "obsoleta gera so o alarme de obsolescencia");
    Aquisicao f = aq(50, std::nullopt);
    f.falhas_sensor.push_back({"TT-01", "FALHA_SENSOR_TT_01", "Sensor TT-01: sensor reporta falha"});
    auto af = avaliar_alarmes(f, false, lim);
    VERIFICAR(tem(af, "FALHA_SENSOR_TT_01") && !tem(af, "AQUISICAO_OBSOLETA") && !tem(af, "TEMPERATURA_ALTA"), "falha de sensor nao vira alarme de limite nem de obsolescencia");
    // Sem leitura valida nao ha alarme de limite (nao se avalia valor inventado).
    VERIFICAR(avaliar_alarmes(aq(std::nullopt, std::nullopt), false, lim).empty(), "sem leituras e sem falhas registradas: nada a avaliar");
    // Serializacao: campos textuais exigidos pelo contrato.
    Json j = alarme_para_json({"NIVEL_BAIXO", "msg", "aviso"});
    VERIFICAR_IGUAL(j.buscar("codigo")->texto_valor(), std::string("NIVEL_BAIXO"), "codigo");
    VERIFICAR_IGUAL(j.buscar("severidade")->texto_valor(), std::string("aviso"), "severidade");
}

int main() {
    politicas_decisoes_diferentes();
    politicas_fronteiras();
    politicas_falha_e_estado_seguro();
    comando();
    alarmes();
    std::cout << "OK: politicas, comando e alarmes (comum, fronteira, falha)\n";
    return 0;
}

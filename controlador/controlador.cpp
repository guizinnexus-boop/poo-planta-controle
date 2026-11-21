#include "controlador.hpp"

#include <algorithm>

#include "aquisicao.hpp"
#include "comando.hpp"
#include "tempo.hpp"

namespace poo {
namespace {

struct EstadoControlador {
    std::string sessao;
    long long ultimo_ciclo = 0;
    long long ultimo_id = 0;      // maior id emitido na sessao
    long long id_pendente = 0;    // id emitido ainda nao confirmado (0 = nenhum)
    Json solicitacao;             // ultima solicitacao (objeto) ou nulo
    long long confirmacao_id = 0; // id a que a confirmacao se refere (0 = nenhum)
    std::string confirmacao_estado = "nenhum";
};

Json estado_para_json(const EstadoControlador& e) {
    Json conf = Json::objeto();
    conf.definir("id", e.confirmacao_id > 0 ? Json::inteiro(e.confirmacao_id) : Json());
    conf.definir("estado", Json::texto(e.confirmacao_estado));
    Json j = Json::objeto();
    j.definir("versao", Json::inteiro(1));
    j.definir("sessao", Json::texto(e.sessao));
    j.definir("ultimo_ciclo", Json::inteiro(e.ultimo_ciclo));
    j.definir("ultimo_id", Json::inteiro(e.ultimo_id));
    j.definir("id_pendente", Json::inteiro(e.id_pendente));
    j.definir("solicitacao", e.solicitacao);
    j.definir("confirmacao", conf);
    return j;
}

bool inteiro_em(const Json& j, const char* chave, long long& out) {
    const Json* v = j.buscar(chave);
    if (v == nullptr || v->tipo() != Json::Tipo::Inteiro || v->inteiro_valor() < 0) return false;
    out = v->inteiro_valor();
    return true;
}

// Estado ausente => estado vazio. Estado invalido => estado vazio + diagnostico (recuperacao).
EstadoControlador carregar_estado(const RepositorioArquivos& repo, std::vector<std::string>& diag) {
    EstadoControlador e;
    auto l = repo.ler(ARQ_ESTADO);
    if (l.status == RepositorioArquivos::Status::Ausente) return e;
    auto invalido = [&](const std::string& m) {
        diag.push_back("ESTADO_CONTROLADOR_INVALIDO: " + m + "; estado reiniciado");
        return EstadoControlador();
    };
    if (l.status == RepositorioArquivos::Status::Invalido) return invalido(l.erro);
    const Json& d = l.documento;
    const Json* sessao = d.buscar("sessao");
    const Json* conf = d.buscar("confirmacao");
    const Json* sol = d.buscar("solicitacao");
    if (sessao == nullptr || sessao->tipo() != Json::Tipo::Texto || conf == nullptr || conf->tipo() != Json::Tipo::Objeto || sol == nullptr)
        return invalido("campos ausentes");
    EstadoControlador novo;
    novo.sessao = sessao->texto_valor();
    if (!inteiro_em(d, "ultimo_ciclo", novo.ultimo_ciclo) || !inteiro_em(d, "ultimo_id", novo.ultimo_id) || !inteiro_em(d, "id_pendente", novo.id_pendente))
        return invalido("contadores invalidos");
    const Json* cid = conf->buscar("id");
    const Json* cest = conf->buscar("estado");
    if (cest == nullptr || cest->tipo() != Json::Tipo::Texto) return invalido("confirmacao invalida");
    novo.confirmacao_id = (cid != nullptr && cid->tipo() == Json::Tipo::Inteiro) ? cid->inteiro_valor() : 0;
    novo.confirmacao_estado = cest->texto_valor();
    novo.solicitacao = *sol;
    return novo;
}

Json solicitacao_json(const ComandoAtuacao& c, const std::string& motivo) {
    Json at = Json::objeto();
    at.definir("bomba", Json::real(c.bomba()));
    at.definir("aquecedor", Json::real(c.aquecedor()));
    Json s = Json::objeto();
    s.definir("id", Json::inteiro(c.id()));
    s.definir("atuadores", at);
    s.definir("motivo", Json::texto(motivo));
    return s;
}

Json montar_telemetria(const Aquisicao& aq, const std::vector<Alarme>& alarmes, const EstadoControlador& est,
                       const std::string& politica, bool obsoleta, long long agora) {
    static const char* const CAMPOS[] = {"versao", "sessao", "ciclo", "timestamp", "leituras", "atuadores", "ultimo_comando", "erro_comando", "motivo_seguro"};
    Json t = Json::objeto();
    for (const char* c : CAMPOS) {
        const Json* v = aq.origem.buscar(c);
        t.definir(c, v ? *v : Json());
    }
    Json lista = Json::lista();
    for (const auto& a : alarmes) lista.adicionar(alarme_para_json(a));
    t.definir("alarmes", lista);

    Json conf = Json::objeto();
    conf.definir("id", est.confirmacao_id > 0 ? Json::inteiro(est.confirmacao_id) : Json());
    conf.definir("estado", Json::texto(est.confirmacao_estado));
    Json ctl = Json::objeto();
    ctl.definir("politica", Json::texto(politica));
    ctl.definir("aquisicao_obsoleta", Json::booleano(obsoleta));
    ctl.definir("solicitacao", est.solicitacao);
    ctl.definir("confirmacao", conf);
    ctl.definir("gerado_em", Json::texto(formatar_utc(agora)));
    t.definir("controlador", ctl);
    return t;
}

}  // namespace

Controlador::Controlador(RepositorioArquivos repositorio, std::unique_ptr<PoliticaControle> politica, ConfigControlador config)
    : repo_(std::move(repositorio)), politica_(std::move(politica)), config_(config) {}

ResultadoCiclo Controlador::executar(long long agora) {
    ResultadoCiclo r;

    auto lido = repo_.ler(ARQ_PLANTA);
    if (lido.status == RepositorioArquivos::Status::Ausente) {
        r.situacao = SituacaoCiclo::Indisponivel;
        r.diagnosticos.push_back("AQUISICAO_INDISPONIVEL: planta.json ausente");
        return r;
    }
    if (lido.status == RepositorioArquivos::Status::Invalido) {
        r.situacao = SituacaoCiclo::Indisponivel;
        r.diagnosticos.push_back("AQUISICAO_INDISPONIVEL: JSON invalido: " + lido.erro);
        return r;
    }
    ResultadoValidacao val = validar_aquisicao(lido.documento);
    if (!val.ok) {
        r.situacao = SituacaoCiclo::Invalida;
        for (const auto& e : val.erros) r.diagnosticos.push_back("AQUISICAO_INVALIDA: " + e);
        return r;
    }
    const Aquisicao& aq = val.aquisicao;
    for (const auto& f : aq.falhas_sensor) r.diagnosticos.push_back(f.codigo + ": " + f.mensagem);

    EstadoControlador estado = carregar_estado(repo_, r.diagnosticos);
    const bool nova_sessao = estado.sessao != aq.sessao;
    if (!nova_sessao && aq.ciclo < estado.ultimo_ciclo) {
        r.situacao = SituacaoCiclo::CicloAnterior;
        r.diagnosticos.push_back("CICLO_ANTERIOR: ciclo " + std::to_string(aq.ciclo) + " menor que o ja processado " + std::to_string(estado.ultimo_ciclo));
        return r;
    }
    const bool repetido = !nova_sessao && aq.ciclo == estado.ultimo_ciclo;
    const bool obsoleta = (agora - aq.epoch) > config_.limite_obsoleto_s;

    if (nova_sessao) {
        estado = EstadoControlador();
        estado.sessao = aq.sessao;
        r.situacao = SituacaoCiclo::SessaoReiniciada;
    } else {
        r.situacao = repetido ? SituacaoCiclo::CicloRepetido : SituacaoCiclo::NovaAquisicao;
    }

    if (!repetido) {
        // 1) Reconciliar o comando anterior com o que a planta confirma neste snapshot.
        if (estado.id_pendente > 0) {
            estado.confirmacao_id = estado.id_pendente;
            if (aq.ultimo_comando >= estado.id_pendente) estado.confirmacao_estado = "confirmado";
            else if (aq.tem_erro_comando) estado.confirmacao_estado = "rejeitado";
            else estado.confirmacao_estado = "pendente";
        } else {
            estado.confirmacao_id = 0;
            estado.confirmacao_estado = "nenhum";
        }

        // 2) Decidir. Aquisicao obsoleta nunca alimenta a politica: estado seguro (0%/0%).
        Decisao d;
        std::string motivo = "politica";
        if (obsoleta) {
            motivo = "aquisicao_obsoleta";
        } else {
            d = politica_->decidir({aq.nivel, aq.temperatura});
        }

        // 3) Emitir no maximo um comando por aquisicao, com id novo e crescente.
        const long long id = std::max(estado.ultimo_id, aq.ultimo_comando) + 1;
        ComandoAtuacao cmd(aq.sessao, id, d.bomba, d.aquecedor);
        estado.ultimo_ciclo = aq.ciclo;
        estado.ultimo_id = id;
        estado.id_pendente = id;
        estado.solicitacao = solicitacao_json(cmd, motivo);
        repo_.gravar(ARQ_ESTADO, estado_para_json(estado));  // estado antes do comando: reinicio nao duplica
        repo_.gravar(ARQ_COMANDO, cmd.para_json());
        r.comando_emitido = true;
        r.id_comando = id;
    }

    // Telemetria sempre reflete a aquisicao validada (reavalia obsolescencia em ciclo repetido).
    auto alarmes = avaliar_alarmes(aq, obsoleta, config_.alarmes);
    repo_.gravar(ARQ_TELEMETRIA, montar_telemetria(aq, alarmes, estado, politica_->nome(), obsoleta, agora));
    r.telemetria_gravada = true;
    r.confirmacao = estado.confirmacao_estado;
    if (obsoleta) r.diagnosticos.push_back("AQUISICAO_OBSOLETA: idade " + std::to_string(agora - aq.epoch) + " s excede " + std::to_string(config_.limite_obsoleto_s) + " s");
    return r;
}

}  // namespace poo

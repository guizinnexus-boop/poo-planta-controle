#include "aquisicao.hpp"

#include <cctype>
#include <cmath>

#include "tempo.hpp"

namespace poo {
namespace {

struct Esperado {
    const char* tag;
    const char* tipo;
    const char* unidade;
};
const Esperado ESP_NIVEL{TAG_NIVEL, "nivel", "%"};
const Esperado ESP_TEMPERATURA{TAG_TEMPERATURA, "temperatura", "C"};

std::string codigo_falha(const std::string& tag) {
    std::string s = "FALHA_SENSOR_";
    for (char c : tag) s += std::isalnum(static_cast<unsigned char>(c)) ? static_cast<char>(std::toupper(static_cast<unsigned char>(c))) : '_';
    return s;
}

const std::string* texto_de(const Json& obj, const char* chave) {
    const Json* j = obj.buscar(chave);
    if (j == nullptr || j->tipo() != Json::Tipo::Texto) return nullptr;
    return &j->texto_valor();
}

bool percentual(const Json* j, double& out) {
    if (j == nullptr || !j->eh_numero()) return false;
    double v = j->numero_valor();
    if (!std::isfinite(v) || v < 0.0 || v > 100.0) return false;
    out = v;
    return true;
}

std::optional<double> avaliar_sensor(const Esperado& esp, const Json* leitura, std::vector<DiagnosticoSensor>& falhas) {
    auto falhar = [&](const std::string& causa) -> std::optional<double> {
        falhas.push_back({esp.tag, codigo_falha(esp.tag), std::string("Sensor ") + esp.tag + ": " + causa});
        return std::nullopt;
    };
    if (leitura == nullptr) return falhar("leitura ausente");
    const std::string* status = texto_de(*leitura, "status");
    if (status == nullptr) return falhar("status ausente ou invalido");
    if (*status == "falha") return falhar("sensor reporta falha");
    if (*status != "operando") return falhar("status desconhecido '" + *status + "'");
    const std::string* tipo = texto_de(*leitura, "tipo");
    if (tipo == nullptr || *tipo != esp.tipo) return falhar("tipo incompativel");
    const std::string* unidade = texto_de(*leitura, "unidade");
    if (unidade == nullptr || *unidade != esp.unidade) return falhar("unidade incompativel");
    const Json* valor = leitura->buscar("valor");
    if (valor == nullptr || !valor->eh_numero()) return falhar("valor ausente ou nao numerico");
    double v = valor->numero_valor();
    if (!std::isfinite(v)) return falhar("valor nao finito");
    if (v < 0.0 || v > 100.0) return falhar("valor fora da faixa [0,100]");
    return v;
}

}  // namespace

ResultadoValidacao validar_aquisicao(const Json& doc) {
    ResultadoValidacao r;
    auto erro = [&](const std::string& m) { r.erros.push_back(m); };

    if (doc.tipo() != Json::Tipo::Objeto) {
        erro("documento nao e um objeto");
        return r;
    }
    Aquisicao& a = r.aquisicao;

    const Json* v = doc.buscar("versao");
    if (v == nullptr || v->tipo() != Json::Tipo::Inteiro || v->inteiro_valor() != 1) erro("versao deve ser o inteiro 1");

    const Json* s = doc.buscar("sessao");
    if (s == nullptr || s->tipo() != Json::Tipo::Texto || s->texto_valor().empty()) erro("sessao deve ser texto nao vazio");
    else a.sessao = s->texto_valor();

    const Json* c = doc.buscar("ciclo");
    if (c == nullptr || c->tipo() != Json::Tipo::Inteiro || c->inteiro_valor() <= 0) erro("ciclo deve ser inteiro positivo");
    else a.ciclo = c->inteiro_valor();

    const Json* t = doc.buscar("timestamp");
    if (t == nullptr || t->tipo() != Json::Tipo::Texto || !analisar_utc(t->texto_valor(), a.epoch)) erro("timestamp deve ser UTC AAAA-MM-DDTHH:MM:SSZ valido");
    else a.timestamp = t->texto_valor();

    const Json* ult = doc.buscar("ultimo_comando");
    if (ult == nullptr || ult->tipo() != Json::Tipo::Inteiro || ult->inteiro_valor() < 0) erro("ultimo_comando deve ser inteiro >= 0");
    else a.ultimo_comando = ult->inteiro_valor();

    const Json* ec = doc.buscar("erro_comando");
    if (ec == nullptr || (!ec->eh_nulo() && ec->tipo() != Json::Tipo::Texto)) erro("erro_comando deve ser texto ou null");
    else a.tem_erro_comando = !ec->eh_nulo();

    const Json* ms = doc.buscar("motivo_seguro");
    if (ms == nullptr || (!ms->eh_nulo() && ms->tipo() != Json::Tipo::Texto)) erro("motivo_seguro deve ser texto ou null");
    else if (!ms->eh_nulo()) a.motivo_seguro = ms->texto_valor();

    const Json* at = doc.buscar("atuadores");
    if (at == nullptr || at->tipo() != Json::Tipo::Objeto) {
        erro("atuadores deve ser objeto");
    } else {
        if (!percentual(at->buscar("bomba"), a.bomba_aplicada)) erro("atuadores.bomba deve ser numero em [0,100]");
        if (!percentual(at->buscar("aquecedor"), a.aquecedor_aplicado)) erro("atuadores.aquecedor deve ser numero em [0,100]");
    }

    const Json* leituras = doc.buscar("leituras");
    const Json* l_nivel = nullptr;
    const Json* l_temp = nullptr;
    if (leituras == nullptr || leituras->tipo() != Json::Tipo::Lista) {
        erro("leituras deve ser lista");
    } else {
        for (const Json& item : leituras->itens()) {
            if (item.tipo() != Json::Tipo::Objeto) { erro("leitura nao e objeto"); continue; }
            const std::string* tag = texto_de(item, "tag");
            if (tag == nullptr) { erro("leitura sem tag textual"); continue; }
            const Json** alvo = (*tag == TAG_NIVEL) ? &l_nivel : (*tag == TAG_TEMPERATURA) ? &l_temp : nullptr;
            if (alvo == nullptr) continue;  // grandeza extra: preservada na origem, ignorada no controle
            if (*alvo != nullptr) erro("tag duplicada " + *tag);
            else *alvo = &item;
        }
    }

    if (!r.erros.empty()) return r;

    a.nivel = avaliar_sensor(ESP_NIVEL, l_nivel, a.falhas_sensor);
    a.temperatura = avaliar_sensor(ESP_TEMPERATURA, l_temp, a.falhas_sensor);
    a.origem = doc;
    r.ok = true;
    return r;
}

}  // namespace poo

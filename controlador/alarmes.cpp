#include "alarmes.hpp"

namespace poo {

std::vector<Alarme> avaliar_alarmes(const Aquisicao& aq, bool obsoleta, const LimitesAlarme& lim) {
    std::vector<Alarme> out;
    if (obsoleta) out.push_back({"AQUISICAO_OBSOLETA", "Aquisicao antiga: dados nao representam o estado atual da planta", "erro"});
    for (const auto& f : aq.falhas_sensor) out.push_back({f.codigo, f.mensagem, "erro"});
    if (aq.nivel.has_value() && *aq.nivel < lim.nivel_baixo) out.push_back({"NIVEL_BAIXO", "Nivel abaixo da faixa definida", "aviso"});
    if (aq.temperatura.has_value() && *aq.temperatura > lim.temperatura_alta) out.push_back({"TEMPERATURA_ALTA", "Temperatura acima da faixa definida", "aviso"});
    return out;
}

Json alarme_para_json(const Alarme& a) {
    Json j = Json::objeto();
    j.definir("codigo", Json::texto(a.codigo));
    j.definir("mensagem", Json::texto(a.mensagem));
    j.definir("severidade", Json::texto(a.severidade));
    return j;
}

}  // namespace poo

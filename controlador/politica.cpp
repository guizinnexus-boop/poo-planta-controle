#include "politica.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace poo {
namespace {

void validar(const ParametrosControle& p) {
    auto faixa = [](double v) { return std::isfinite(v) && v >= 0.0 && v <= 100.0; };
    if (!faixa(p.alvo_nivel) || !faixa(p.alvo_temperatura)) throw std::invalid_argument("alvo fora de [0,100]");
    if (!(std::isfinite(p.ganho_nivel) && p.ganho_nivel > 0.0) || !(std::isfinite(p.ganho_temperatura) && p.ganho_temperatura > 0.0))
        throw std::invalid_argument("ganho deve ser positivo");
}

}  // namespace

PoliticaLigaDesliga::PoliticaLigaDesliga(ParametrosControle p) : p_(p) { validar(p_); }

Decisao PoliticaLigaDesliga::decidir(const EntradaControle& e) const {
    Decisao d;
    if (e.nivel.has_value()) d.bomba = (*e.nivel < p_.alvo_nivel) ? 100.0 : 0.0;
    if (e.temperatura.has_value()) d.aquecedor = (*e.temperatura < p_.alvo_temperatura) ? 100.0 : 0.0;
    return d;
}

PoliticaProporcional::PoliticaProporcional(ParametrosControle p) : p_(p) { validar(p_); }

Decisao PoliticaProporcional::decidir(const EntradaControle& e) const {
    auto limitar = [](double v) { return std::clamp(v, 0.0, 100.0); };
    Decisao d;
    if (e.nivel.has_value()) d.bomba = limitar(p_.ganho_nivel * (p_.alvo_nivel - *e.nivel));
    if (e.temperatura.has_value()) d.aquecedor = limitar(p_.ganho_temperatura * (p_.alvo_temperatura - *e.temperatura));
    return d;
}

std::unique_ptr<PoliticaControle> criar_politica(const std::string& nome, const ParametrosControle& p) {
    if (nome == "liga-desliga") return std::make_unique<PoliticaLigaDesliga>(p);
    if (nome == "proporcional") return std::make_unique<PoliticaProporcional>(p);
    return nullptr;
}

}  // namespace poo

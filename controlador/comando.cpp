#include "comando.hpp"

#include <cmath>
#include <stdexcept>

namespace poo {
namespace {
bool percentual_valido(double v) { return std::isfinite(v) && v >= 0.0 && v <= 100.0; }
}

ComandoAtuacao::ComandoAtuacao(std::string sessao, long long id, double bomba, double aquecedor)
    : sessao_(std::move(sessao)), id_(id), bomba_(bomba), aquecedor_(aquecedor) {
    if (sessao_.empty()) throw std::invalid_argument("sessao vazia");
    if (id_ <= 0) throw std::invalid_argument("id deve ser positivo");
    if (!percentual_valido(bomba_)) throw std::invalid_argument("bomba fora de [0,100]");
    if (!percentual_valido(aquecedor_)) throw std::invalid_argument("aquecedor fora de [0,100]");
}

Json ComandoAtuacao::para_json() const {
    Json atuadores = Json::objeto();
    atuadores.definir("bomba", Json::real(bomba_));
    atuadores.definir("aquecedor", Json::real(aquecedor_));
    Json j = Json::objeto();
    j.definir("versao", Json::inteiro(1));
    j.definir("sessao", Json::texto(sessao_));
    j.definir("id", Json::inteiro(id_));
    j.definir("atuadores", atuadores);
    return j;
}

}  // namespace poo

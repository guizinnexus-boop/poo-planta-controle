// Solicitacao de atuacao. O construtor protege a invariante do contrato.
#pragma once
#include <string>

#include "json.hpp"

namespace poo {

class ComandoAtuacao {
public:
    // Lanca std::invalid_argument se sessao vazia, id <= 0 ou percentuais fora de [0,100]/nao finitos.
    ComandoAtuacao(std::string sessao, long long id, double bomba, double aquecedor);

    const std::string& sessao() const { return sessao_; }
    long long id() const { return id_; }
    double bomba() const { return bomba_; }
    double aquecedor() const { return aquecedor_; }

    Json para_json() const;  // formato de comando_planta.json

private:
    std::string sessao_;
    long long id_;
    double bomba_;
    double aquecedor_;
};

}  // namespace poo

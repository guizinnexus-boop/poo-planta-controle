// Regras de alarme. Fronteiras explicitas: nivel < 20 e temperatura > 80 (estritos).
#pragma once
#include <string>
#include <vector>

#include "aquisicao.hpp"
#include "json.hpp"

namespace poo {

struct Alarme {
    std::string codigo;
    std::string mensagem;
    std::string severidade;  // "aviso" | "erro"
};

struct LimitesAlarme {
    double nivel_baixo = 20.0;        // alarme se nivel <  limite (igual ao limite NAO alarma)
    double temperatura_alta = 80.0;   // alarme se temp  >  limite (igual ao limite NAO alarma)
};

// Ordem: obsolescencia, falhas de sensor, NIVEL_BAIXO, TEMPERATURA_ALTA.
// Grandeza sem leitura valida nao gera alarme de limite (so o de falha do sensor).
std::vector<Alarme> avaliar_alarmes(const Aquisicao& aq, bool obsoleta, const LimitesAlarme& limites);

Json alarme_para_json(const Alarme& a);

}  // namespace poo

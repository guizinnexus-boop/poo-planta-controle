// Validacao da aquisicao (planta.json). Leitura invalida nunca vira medicao.
#pragma once
#include <optional>
#include <string>
#include <vector>

#include "json.hpp"

namespace poo {

inline constexpr const char* TAG_NIVEL = "LT-01";
inline constexpr const char* TAG_TEMPERATURA = "TT-01";

struct DiagnosticoSensor {
    std::string tag;
    std::string codigo;    // ex.: FALHA_SENSOR_TT_01
    std::string mensagem;  // causa legivel
};

struct Aquisicao {
    std::string sessao;
    long long ciclo = 0;
    std::string timestamp;
    long long epoch = 0;
    std::optional<double> nivel;        // vazio se LT-01 nao tem leitura valida
    std::optional<double> temperatura;  // vazio se TT-01 nao tem leitura valida
    double bomba_aplicada = 0.0;
    double aquecedor_aplicado = 0.0;
    long long ultimo_comando = 0;
    bool tem_erro_comando = false;
    std::string motivo_seguro;
    std::vector<DiagnosticoSensor> falhas_sensor;
    Json origem;  // documento original, preservado para a telemetria
};

struct ResultadoValidacao {
    bool ok = false;
    std::vector<std::string> erros;  // problemas do documento inteiro
    Aquisicao aquisicao;             // significativa apenas se ok
};

// Erros do documento (versao, sessao, ciclo, timestamp, estrutura) rejeitam tudo.
// Problemas em uma leitura rejeitam so aquela grandeza (falhas_sensor).
ResultadoValidacao validar_aquisicao(const Json& documento);

}  // namespace poo

// Orquestra um ciclo: ler aquisicao -> validar -> decidir -> comandar -> telemetria.
#pragma once
#include <memory>
#include <string>
#include <vector>

#include "alarmes.hpp"
#include "politica.hpp"
#include "repositorio.hpp"

namespace poo {

struct ConfigControlador {
    long long limite_obsoleto_s = 10;  // idade maxima da aquisicao (segundos); diferente do timeout de 5 ciclos da planta
    LimitesAlarme alarmes;
};

enum class SituacaoCiclo {
    NovaAquisicao,      // ciclo novo na mesma sessao: decide e comanda
    SessaoReiniciada,   // sessao diferente: estado anterior abandonado, decide e comanda
    CicloRepetido,      // mesmo sessao/ciclo: NAO comanda, so atualiza telemetria
    CicloAnterior,      // ciclo menor que o ja processado: ignorado
    Indisponivel,       // planta.json ausente ou JSON invalido
    Invalida,           // documento viola o contrato
};

struct ResultadoCiclo {
    SituacaoCiclo situacao = SituacaoCiclo::Indisponivel;
    bool comando_emitido = false;
    long long id_comando = 0;
    bool telemetria_gravada = false;
    std::string confirmacao = "nenhum";  // nenhum | confirmado | pendente | rejeitado
    std::vector<std::string> diagnosticos;

    // Falhas de entrada exigem retorno diferente de zero na CLI.
    bool entrada_ok() const {
        return situacao != SituacaoCiclo::Indisponivel && situacao != SituacaoCiclo::Invalida && situacao != SituacaoCiclo::CicloAnterior;
    }
};

class Controlador {
public:
    Controlador(RepositorioArquivos repositorio, std::unique_ptr<PoliticaControle> politica, ConfigControlador config = {});

    // agora_utc: epoch em segundos (injetado para testes deterministicos).
    ResultadoCiclo executar(long long agora_utc);

private:
    RepositorioArquivos repo_;
    std::unique_ptr<PoliticaControle> politica_;
    ConfigControlador config_;
};

}  // namespace poo

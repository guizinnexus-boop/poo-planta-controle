// Repository: unico ponto de acesso aos JSON do diretorio de integracao.
// Escrita atomica: arquivo temporario no mesmo diretorio + rename.
#pragma once
#include <string>

#include "json.hpp"

namespace poo {

inline constexpr const char* ARQ_PLANTA = "planta.json";
inline constexpr const char* ARQ_COMANDO = "comando_planta.json";
inline constexpr const char* ARQ_TELEMETRIA = "telemetria.json";
inline constexpr const char* ARQ_ESTADO = "controlador_estado.json";

class RepositorioArquivos {
public:
    enum class Status { Ok, Ausente, Invalido };
    struct Leitura {
        Status status = Status::Ausente;
        Json documento;
        std::string erro;  // preenchido se Invalido
    };

    explicit RepositorioArquivos(std::string diretorio);

    Leitura ler(const std::string& nome) const;
    // Lanca std::runtime_error se nao conseguir gravar; o destino anterior fica intacto.
    void gravar(const std::string& nome, const Json& documento) const;
    std::string caminho(const std::string& nome) const;

private:
    std::string diretorio_;
};

}  // namespace poo

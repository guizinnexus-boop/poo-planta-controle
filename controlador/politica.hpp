// Strategy: interface de politica de controle e duas estrategias distintas.
// Politicas sao puras (sem estado, sem aleatoriedade, sem arquivo, sem Streamlit).
#pragma once
#include <memory>
#include <optional>
#include <string>

namespace poo {

struct EntradaControle {
    std::optional<double> nivel;        // vazio = sem leitura valida
    std::optional<double> temperatura;  // vazio = sem leitura valida
};

struct Decisao {
    double bomba = 0.0;      // percentual solicitado
    double aquecedor = 0.0;  // percentual solicitado
};

struct ParametrosControle {
    double alvo_nivel = 60.0;         // %
    double alvo_temperatura = 40.0;   // C
    double ganho_nivel = 5.0;         // % de bomba por ponto de erro (proporcional)
    double ganho_temperatura = 10.0;  // % de aquecedor por grau de erro (proporcional)
};

class PoliticaControle {
public:
    virtual ~PoliticaControle() = default;
    virtual std::string nome() const = 0;
    // Grandeza sem leitura valida => atuador dependente fica em 0 (estado seguro).
    virtual Decisao decidir(const EntradaControle& entrada) const = 0;
};

// Liga/desliga: 100% abaixo do alvo, 0% no alvo ou acima.
class PoliticaLigaDesliga : public PoliticaControle {
public:
    explicit PoliticaLigaDesliga(ParametrosControle p = {});  // valida parametros
    std::string nome() const override { return "liga-desliga"; }
    Decisao decidir(const EntradaControle& entrada) const override;

private:
    ParametrosControle p_;
};

// Proporcional: percentual = ganho * (alvo - medida), limitado a [0,100].
class PoliticaProporcional : public PoliticaControle {
public:
    explicit PoliticaProporcional(ParametrosControle p = {});  // valida parametros
    std::string nome() const override { return "proporcional"; }
    Decisao decidir(const EntradaControle& entrada) const override;

private:
    ParametrosControle p_;
};

// Fabrica simples por nome; retorna nullptr se o nome for desconhecido.
std::unique_ptr<PoliticaControle> criar_politica(const std::string& nome, const ParametrosControle& p = {});

}  // namespace poo

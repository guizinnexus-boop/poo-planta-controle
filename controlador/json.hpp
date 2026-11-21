// Representacao minima de JSON para o controlador (sem dependencias externas).
// Distingue inteiro de real e booleano de numero, como o contrato exige.
#pragma once
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace poo {

class ErroJson : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

class Json {
public:
    enum class Tipo { Nulo, Booleano, Inteiro, Real, Texto, Lista, Objeto };

    Json() = default;  // nulo
    static Json booleano(bool v);
    static Json inteiro(long long v);
    static Json real(double v);  // lanca ErroJson se nao for finito
    static Json texto(std::string v);
    static Json lista();
    static Json objeto();

    Tipo tipo() const { return tipo_; }
    bool eh_nulo() const { return tipo_ == Tipo::Nulo; }
    bool eh_numero() const { return tipo_ == Tipo::Inteiro || tipo_ == Tipo::Real; }

    bool booleano_valor() const;
    long long inteiro_valor() const;
    double numero_valor() const;  // aceita Inteiro ou Real
    const std::string& texto_valor() const;
    const std::vector<Json>& itens() const;
    const std::vector<std::pair<std::string, Json>>& campos() const;

    const Json* buscar(const std::string& chave) const;  // nullptr se ausente
    void adicionar(Json item);                           // somente em lista
    void definir(const std::string& chave, Json valor);  // somente em objeto; substitui

    std::string serializar() const;
    static Json analisar(const std::string& texto);  // lanca ErroJson

private:
    Tipo tipo_ = Tipo::Nulo;
    bool bool_ = false;
    long long int_ = 0;
    double real_ = 0.0;
    std::string texto_;
    std::vector<Json> itens_;
    std::vector<std::pair<std::string, Json>> campos_;
};

}  // namespace poo

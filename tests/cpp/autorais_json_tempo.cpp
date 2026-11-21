// Testa Json (analise/serializacao) e tempo (timestamps UTC).
#include "../../controlador/json.hpp"
#include "../../controlador/tempo.hpp"
#include "teste_util.hpp"

using poo::Json;
using poo::ErroJson;

static void json_comum() {
    Json j = Json::analisar(R"({"a": 1, "b": 2.5, "c": "x\ny", "d": [true, null], "e": {"k": -3}})");
    VERIFICAR_IGUAL(j.buscar("a")->tipo(), Json::Tipo::Inteiro, "1 deve ser inteiro");
    VERIFICAR_IGUAL(j.buscar("b")->tipo(), Json::Tipo::Real, "2.5 deve ser real");
    VERIFICAR_IGUAL(j.buscar("c")->texto_valor(), std::string("x\ny"), "escape \\n");
    VERIFICAR_IGUAL(j.buscar("d")->itens().size(), size_t(2), "lista com 2 itens");
    VERIFICAR(j.buscar("d")->itens()[0].booleano_valor(), "true");
    VERIFICAR(j.buscar("d")->itens()[1].eh_nulo(), "null");
    VERIFICAR_IGUAL(j.buscar("e")->buscar("k")->inteiro_valor(), -3LL, "inteiro negativo aninhado");
    VERIFICAR(j.buscar("ausente") == nullptr, "chave ausente => nullptr");
    // Ida e volta preserva tipos e ordem.
    Json k = Json::analisar(j.serializar());
    VERIFICAR_IGUAL(k.serializar(), j.serializar(), "serializar/analisar deve ser estavel");
    VERIFICAR_IGUAL(Json::real(75.0).serializar(), std::string("75.0"), "real inteiro mantem .0");
    VERIFICAR_IGUAL(Json::real(0.1).serializar(), std::string("0.1"), "0.1 sem ruido binario");
}

static void json_fronteira() {
    VERIFICAR_IGUAL(Json::analisar("0").tipo(), Json::Tipo::Inteiro, "zero e inteiro");
    VERIFICAR_IGUAL(Json::analisar("1e2").tipo(), Json::Tipo::Real, "expoente => real");
    VERIFICAR_IGUAL(Json::analisar("true").tipo(), Json::Tipo::Booleano, "booleano nao e numero");
    VERIFICAR(!Json::analisar("true").eh_numero(), "booleano nao e numero");
    VERIFICAR_IGUAL(Json::analisar("\"\\u00e9\"").texto_valor(), std::string("\xc3\xa9"), "\\u00e9 vira UTF-8");
    VERIFICAR_IGUAL(Json::analisar("  [ ]  ").itens().size(), size_t(0), "lista vazia com espacos");
    VERIFICAR_IGUAL(Json::analisar("99999999999999999999").tipo(), Json::Tipo::Real, "inteiro gigante vira real");
    Json o = Json::objeto();
    o.definir("x", Json::inteiro(1));
    o.definir("x", Json::inteiro(2));
    VERIFICAR_IGUAL(o.campos().size(), size_t(1), "definir substitui chave existente");
    VERIFICAR_IGUAL(o.buscar("x")->inteiro_valor(), 2LL, "valor substituido");
}

static void json_falha() {
    const char* invalidos[] = {"", "{", "{\"a\":}", "[1,]", "NaN", "Infinity", "01", "1.", "\"abc", "{\"a\":1} lixo", "[1 2]", "{'a':1}", "\"\\x\""};
    for (const char* t : invalidos) {
        bool lancou = false;
        try { Json::analisar(t); } catch (const ErroJson&) { lancou = true; }
        VERIFICAR(lancou, std::string("deveria rejeitar: ") + t);
    }
    VERIFICAR_LANCA(Json::real(INFINITY), ErroJson, "real infinito rejeitado");
    VERIFICAR_LANCA(Json::inteiro(1).texto_valor(), ErroJson, "acesso com tipo errado lanca");
    VERIFICAR_LANCA(Json::analisar("1e999"), ErroJson, "numero fora do intervalo rejeitado");
}

static void tempo_testes() {
    long long e = 0;
    VERIFICAR(poo::analisar_utc("1970-01-01T00:00:00Z", e), "epoca valida");
    VERIFICAR_IGUAL(e, 0LL, "epoca = 0");
    VERIFICAR(poo::analisar_utc("2024-02-29T12:00:00Z", e), "29/02 de ano bissexto");
    VERIFICAR_IGUAL(e, 1709208000LL, "valor conhecido 2024-02-29T12:00:00Z");
    VERIFICAR_IGUAL(poo::formatar_utc(1709208000LL), std::string("2024-02-29T12:00:00Z"), "formatar inverte analisar");
    // Fronteiras e falhas.
    VERIFICAR(!poo::analisar_utc("2023-02-29T12:00:00Z", e), "29/02 de ano comum invalido");
    VERIFICAR(!poo::analisar_utc("2024-13-01T00:00:00Z", e), "mes 13 invalido");
    VERIFICAR(!poo::analisar_utc("2024-01-01T24:00:00Z", e), "hora 24 invalida");
    VERIFICAR(!poo::analisar_utc("2024-01-01T23:59:60Z", e), "segundo 60 invalido");
    VERIFICAR(poo::analisar_utc("2024-01-01T23:59:59Z", e), "23:59:59 valido");
    VERIFICAR(!poo::analisar_utc("2024-01-01 00:00:00Z", e), "separador errado");
    VERIFICAR(!poo::analisar_utc("2024-01-01T00:00:00", e), "sem Z");
    VERIFICAR(!poo::analisar_utc("2024-01-01T00:00:00+00:00", e), "offset nao aceito");
    long long antes = e;
    VERIFICAR(!poo::analisar_utc("lixo", e), "texto qualquer");
    VERIFICAR_IGUAL(e, antes, "falha nao altera a saida");
}

int main() {
    json_comum();
    json_fronteira();
    json_falha();
    tempo_testes();
    std::cout << "OK: Json e tempo (comum, fronteira, falha)\n";
    return 0;
}

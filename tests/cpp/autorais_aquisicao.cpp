// Testa validar_aquisicao: documento valido, falha por sensor e rejeicao do documento.
#include "../../controlador/aquisicao.hpp"
#include "teste_util.hpp"

using poo::Json;

static const char* BASE = R"({"versao":1,"sessao":"s1","ciclo":4,"timestamp":"2026-01-01T10:00:00Z",
 "leituras":[{"tag":"LT-01","tipo":"nivel","valor":55.5,"unidade":"%","status":"operando"},
             {"tag":"TT-01","tipo":"temperatura","valor":30,"unidade":"C","status":"operando"}],
 "atuadores":{"bomba":10.0,"aquecedor":0},"ultimo_comando":2,"erro_comando":null,"motivo_seguro":null})";

// Substitui um trecho do documento base para criar variantes.
static poo::ResultadoValidacao validar_com(const std::string& de, const std::string& para) {
    std::string t = BASE;
    size_t p = t.find(de);
    VERIFICAR(p != std::string::npos, "trecho a substituir nao encontrado: " + de);
    t.replace(p, de.size(), para);
    return poo::validar_aquisicao(Json::analisar(t));
}

static void comum() {
    auto r = poo::validar_aquisicao(Json::analisar(BASE));
    VERIFICAR(r.ok && r.erros.empty(), "documento valido deve passar");
    VERIFICAR_IGUAL(r.aquisicao.sessao, std::string("s1"), "sessao");
    VERIFICAR_IGUAL(r.aquisicao.ciclo, 4LL, "ciclo");
    VERIFICAR(r.aquisicao.nivel.has_value() && *r.aquisicao.nivel == 55.5, "nivel 55.5");
    VERIFICAR(r.aquisicao.temperatura.has_value() && *r.aquisicao.temperatura == 30.0, "temperatura 30 (inteiro aceito)");
    VERIFICAR_PROXIMO(r.aquisicao.bomba_aplicada, 10.0, "bomba aplicada");
    VERIFICAR_IGUAL(r.aquisicao.ultimo_comando, 2LL, "ultimo_comando");
    VERIFICAR(!r.aquisicao.tem_erro_comando, "erro_comando nulo");
    VERIFICAR(r.aquisicao.falhas_sensor.empty(), "sem falhas de sensor");
    VERIFICAR(r.aquisicao.origem.buscar("leituras") != nullptr, "origem preservada");
}

static void fronteira() {
    auto r = validar_com("\"valor\":55.5", "\"valor\":0");
    VERIFICAR(r.ok && r.aquisicao.nivel.has_value() && *r.aquisicao.nivel == 0.0, "valor 0 e valido");
    r = validar_com("\"valor\":55.5", "\"valor\":100");
    VERIFICAR(r.ok && r.aquisicao.nivel.has_value(), "valor 100 e valido");
    r = validar_com("\"valor\":55.5", "\"valor\":100.0001");
    VERIFICAR(r.ok && !r.aquisicao.nivel.has_value(), "100.0001 rejeita so o nivel");
    r = validar_com("\"valor\":55.5", "\"valor\":-0.0001");
    VERIFICAR(r.ok && !r.aquisicao.nivel.has_value(), "negativo rejeita so o nivel");
    r = validar_com("\"ciclo\":4", "\"ciclo\":1");
    VERIFICAR(r.ok, "ciclo 1 e o menor valido");
    r = validar_com("\"ciclo\":4", "\"ciclo\":0");
    VERIFICAR(!r.ok, "ciclo 0 invalido");
    r = validar_com("\"ultimo_comando\":2", "\"ultimo_comando\":0");
    VERIFICAR(r.ok, "ultimo_comando 0 e valido");
}

static void falha_por_sensor() {
    // TT-01 com falha explicita: so a temperatura fica indisponivel; nivel segue valido.
    auto r = validar_com(R"("valor":30,"unidade":"C","status":"operando")", R"("valor":null,"unidade":"C","status":"falha")");
    VERIFICAR(r.ok, "falha de um sensor nao rejeita o documento");
    VERIFICAR(!r.aquisicao.temperatura.has_value(), "temperatura indisponivel");
    VERIFICAR(r.aquisicao.nivel.has_value(), "nivel continua valido (grandezas independentes)");
    VERIFICAR_IGUAL(r.aquisicao.falhas_sensor.size(), size_t(1), "uma falha registrada");
    VERIFICAR_IGUAL(r.aquisicao.falhas_sensor[0].codigo, std::string("FALHA_SENSOR_TT_01"), "codigo estavel");
    VERIFICAR_IGUAL(r.aquisicao.falhas_sensor[0].tag, std::string("TT-01"), "tag identificada");

    r = validar_com("\"unidade\":\"%\"", "\"unidade\":\"C\"");
    VERIFICAR(r.ok && !r.aquisicao.nivel.has_value(), "unidade incompativel rejeita a leitura");
    r = validar_com("\"tipo\":\"temperatura\"", "\"tipo\":\"nivel\"");
    VERIFICAR(r.ok && !r.aquisicao.temperatura.has_value(), "tipo incompativel rejeita a leitura");
    r = validar_com("\"valor\":55.5", "\"valor\":\"55.5\"");
    VERIFICAR(r.ok && !r.aquisicao.nivel.has_value(), "valor textual rejeitado");
    r = validar_com("\"valor\":55.5", "\"valor\":true");
    VERIFICAR(r.ok && !r.aquisicao.nivel.has_value(), "booleano nao e valor numerico");
    r = validar_com("\"status\":\"operando\"},", "\"status\":\"quebrado\"},");
    VERIFICAR(r.ok && !r.aquisicao.nivel.has_value(), "status desconhecido rejeitado");
    // Leitura ausente: remove a tag de temperatura trocando-a por grandeza desconhecida.
    r = validar_com("\"tag\":\"TT-01\"", "\"tag\":\"XX-99\"");
    VERIFICAR(r.ok && !r.aquisicao.temperatura.has_value(), "tag ausente => sem temperatura");
    VERIFICAR_IGUAL(r.aquisicao.falhas_sensor[0].codigo, std::string("FALHA_SENSOR_TT_01"), "falha da tag ausente");
    // Ambos falham.
    r = validar_com("\"valor\":55.5", "\"valor\":null");
    VERIFICAR(r.ok && !r.aquisicao.nivel.has_value() && r.aquisicao.temperatura.has_value(), "nulo com status operando rejeita so o nivel");
}

static void rejeicao_do_documento() {
    VERIFICAR(!validar_com("\"versao\":1", "\"versao\":2").ok, "versao 2");
    VERIFICAR(!validar_com("\"versao\":1", "\"versao\":true").ok, "booleano nao e inteiro 1");
    VERIFICAR(!validar_com("\"versao\":1", "\"versao\":1.0").ok, "1.0 nao e o inteiro 1");
    VERIFICAR(!validar_com("\"sessao\":\"s1\"", "\"sessao\":\"\"").ok, "sessao vazia");
    VERIFICAR(!validar_com("\"ciclo\":4", "\"ciclo\":4.0").ok, "ciclo real");
    VERIFICAR(!validar_com("\"timestamp\":\"2026-01-01T10:00:00Z\"", "\"timestamp\":\"2026-01-01 10:00:00\"").ok, "timestamp fora do formato");
    VERIFICAR(!validar_com("\"bomba\":10.0", "\"bomba\":101").ok, "bomba aplicada > 100");
    VERIFICAR(!validar_com("\"aquecedor\":0", "\"aquecedor\":\"0\"").ok, "aquecedor textual");
    VERIFICAR(!validar_com("\"ultimo_comando\":2", "\"ultimo_comando\":-1").ok, "ultimo_comando negativo");
    VERIFICAR(!validar_com("\"erro_comando\":null", "\"erro_comando\":5").ok, "erro_comando numerico");
    VERIFICAR(!poo::validar_aquisicao(Json::analisar("[]")).ok, "raiz nao e objeto");
    VERIFICAR(!poo::validar_aquisicao(Json::analisar("{}")).ok, "objeto vazio");
    auto dup = validar_com("\"tag\":\"TT-01\"", "\"tag\":\"LT-01\"");
    VERIFICAR(!dup.ok, "tag duplicada rejeita o documento");
    auto r = validar_com("\"versao\":1", "\"versao\":9");
    VERIFICAR(!r.erros.empty(), "rejeicao traz diagnostico");
}

int main() {
    comum();
    fronteira();
    falha_por_sensor();
    rejeicao_do_documento();
    std::cout << "OK: validar_aquisicao (comum, fronteira, falha por sensor, rejeicao)\n";
    return 0;
}

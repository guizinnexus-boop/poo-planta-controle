#include "json.hpp"

#include <cerrno>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace poo {

Json Json::booleano(bool v) { Json j; j.tipo_ = Tipo::Booleano; j.bool_ = v; return j; }
Json Json::inteiro(long long v) { Json j; j.tipo_ = Tipo::Inteiro; j.int_ = v; return j; }
Json Json::real(double v) {
    if (!std::isfinite(v)) throw ErroJson("numero real nao finito");
    Json j; j.tipo_ = Tipo::Real; j.real_ = v; return j;
}
Json Json::texto(std::string v) { Json j; j.tipo_ = Tipo::Texto; j.texto_ = std::move(v); return j; }
Json Json::lista() { Json j; j.tipo_ = Tipo::Lista; return j; }
Json Json::objeto() { Json j; j.tipo_ = Tipo::Objeto; return j; }

bool Json::booleano_valor() const {
    if (tipo_ != Tipo::Booleano) throw ErroJson("valor nao e booleano");
    return bool_;
}
long long Json::inteiro_valor() const {
    if (tipo_ != Tipo::Inteiro) throw ErroJson("valor nao e inteiro");
    return int_;
}
double Json::numero_valor() const {
    if (tipo_ == Tipo::Inteiro) return static_cast<double>(int_);
    if (tipo_ == Tipo::Real) return real_;
    throw ErroJson("valor nao e numero");
}
const std::string& Json::texto_valor() const {
    if (tipo_ != Tipo::Texto) throw ErroJson("valor nao e texto");
    return texto_;
}
const std::vector<Json>& Json::itens() const {
    if (tipo_ != Tipo::Lista) throw ErroJson("valor nao e lista");
    return itens_;
}
const std::vector<std::pair<std::string, Json>>& Json::campos() const {
    if (tipo_ != Tipo::Objeto) throw ErroJson("valor nao e objeto");
    return campos_;
}
const Json* Json::buscar(const std::string& chave) const {
    if (tipo_ != Tipo::Objeto) return nullptr;
    for (const auto& c : campos_) {
        if (c.first == chave) return &c.second;
    }
    return nullptr;
}
void Json::adicionar(Json item) {
    if (tipo_ != Tipo::Lista) throw ErroJson("adicionar exige lista");
    itens_.push_back(std::move(item));
}
void Json::definir(const std::string& chave, Json valor) {
    if (tipo_ != Tipo::Objeto) throw ErroJson("definir exige objeto");
    for (auto& c : campos_) {
        if (c.first == chave) { c.second = std::move(valor); return; }
    }
    campos_.emplace_back(chave, std::move(valor));
}

// ---------------------------------------------------------------- serializar
namespace {

std::string formatar_real(double v) {
    char buf[48];
    for (int p = 15; p <= 17; ++p) {
        std::snprintf(buf, sizeof buf, "%.*g", p, v);
        if (std::strtod(buf, nullptr) == v) break;
    }
    std::string s = buf;
    if (s.find_first_of(".e") == std::string::npos) s += ".0";
    return s;
}

void escrever_texto(const std::string& t, std::string& out) {
    out += '"';
    for (unsigned char c : t) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
}

void escrever(const Json& j, std::string& out) {
    switch (j.tipo()) {
        case Json::Tipo::Nulo: out += "null"; break;
        case Json::Tipo::Booleano: out += j.booleano_valor() ? "true" : "false"; break;
        case Json::Tipo::Inteiro: out += std::to_string(j.inteiro_valor()); break;
        case Json::Tipo::Real: out += formatar_real(j.numero_valor()); break;
        case Json::Tipo::Texto: escrever_texto(j.texto_valor(), out); break;
        case Json::Tipo::Lista: {
            out += '[';
            bool primeiro = true;
            for (const auto& i : j.itens()) {
                if (!primeiro) out += ',';
                primeiro = false;
                escrever(i, out);
            }
            out += ']';
            break;
        }
        case Json::Tipo::Objeto: {
            out += '{';
            bool primeiro = true;
            for (const auto& c : j.campos()) {
                if (!primeiro) out += ',';
                primeiro = false;
                escrever_texto(c.first, out);
                out += ':';
                escrever(c.second, out);
            }
            out += '}';
            break;
        }
    }
}

// ------------------------------------------------------------------ analisar
class Analisador {
public:
    explicit Analisador(const std::string& t) : t_(t) {}

    Json documento() {
        Json j = valor(0);
        espaco();
        if (pos_ != t_.size()) falhar("conteudo apos o fim do documento");
        return j;
    }

private:
    const std::string& t_;
    size_t pos_ = 0;

    [[noreturn]] void falhar(const std::string& m) const {
        throw ErroJson(m + " (posicao " + std::to_string(pos_) + ")");
    }
    void espaco() {
        while (pos_ < t_.size() && (t_[pos_] == ' ' || t_[pos_] == '\t' || t_[pos_] == '\n' || t_[pos_] == '\r')) ++pos_;
    }
    bool consumir(const char* lit) {
        size_t n = std::char_traits<char>::length(lit);
        if (t_.compare(pos_, n, lit) == 0) { pos_ += n; return true; }
        return false;
    }

    Json valor(int profundidade) {
        if (profundidade > 64) falhar("aninhamento excessivo");
        espaco();
        if (pos_ >= t_.size()) falhar("fim inesperado");
        char c = t_[pos_];
        if (c == '{') return objeto(profundidade);
        if (c == '[') return lista(profundidade);
        if (c == '"') return Json::texto(texto());
        if (consumir("true")) return Json::booleano(true);
        if (consumir("false")) return Json::booleano(false);
        if (consumir("null")) return Json();
        if (c == '-' || (c >= '0' && c <= '9')) return numero();
        falhar("token inesperado");
    }

    Json objeto(int profundidade) {
        ++pos_;
        Json o = Json::objeto();
        espaco();
        if (pos_ < t_.size() && t_[pos_] == '}') { ++pos_; return o; }
        while (true) {
            espaco();
            if (pos_ >= t_.size() || t_[pos_] != '"') falhar("chave de objeto esperada");
            std::string chave = texto();
            espaco();
            if (pos_ >= t_.size() || t_[pos_] != ':') falhar("':' esperado");
            ++pos_;
            o.definir(chave, valor(profundidade + 1));
            espaco();
            if (pos_ >= t_.size()) falhar("objeto nao terminado");
            if (t_[pos_] == ',') { ++pos_; continue; }
            if (t_[pos_] == '}') { ++pos_; return o; }
            falhar("',' ou '}' esperado");
        }
    }

    Json lista(int profundidade) {
        ++pos_;
        Json l = Json::lista();
        espaco();
        if (pos_ < t_.size() && t_[pos_] == ']') { ++pos_; return l; }
        while (true) {
            l.adicionar(valor(profundidade + 1));
            espaco();
            if (pos_ >= t_.size()) falhar("lista nao terminada");
            if (t_[pos_] == ',') { ++pos_; continue; }
            if (t_[pos_] == ']') { ++pos_; return l; }
            falhar("',' ou ']' esperado");
        }
    }

    unsigned hex4() {
        if (pos_ + 4 > t_.size()) falhar("escape \\u incompleto");
        unsigned v = 0;
        for (int i = 0; i < 4; ++i) {
            char c = t_[pos_++];
            v <<= 4;
            if (c >= '0' && c <= '9') v |= c - '0';
            else if (c >= 'a' && c <= 'f') v |= c - 'a' + 10;
            else if (c >= 'A' && c <= 'F') v |= c - 'A' + 10;
            else falhar("digito hexadecimal invalido");
        }
        return v;
    }

    static void utf8(unsigned cp, std::string& out) {
        if (cp < 0x80) out += static_cast<char>(cp);
        else if (cp < 0x800) { out += static_cast<char>(0xC0 | (cp >> 6)); out += static_cast<char>(0x80 | (cp & 0x3F)); }
        else if (cp < 0x10000) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    std::string texto() {
        ++pos_;  // aspas de abertura
        std::string out;
        while (true) {
            if (pos_ >= t_.size()) falhar("texto nao terminado");
            unsigned char c = static_cast<unsigned char>(t_[pos_++]);
            if (c == '"') return out;
            if (c < 0x20) falhar("caractere de controle em texto");
            if (c != '\\') { out += static_cast<char>(c); continue; }
            if (pos_ >= t_.size()) falhar("escape incompleto");
            char e = t_[pos_++];
            switch (e) {
                case '"': out += '"'; break;
                case '\\': out += '\\'; break;
                case '/': out += '/'; break;
                case 'b': out += '\b'; break;
                case 'f': out += '\f'; break;
                case 'n': out += '\n'; break;
                case 'r': out += '\r'; break;
                case 't': out += '\t'; break;
                case 'u': {
                    unsigned cp = hex4();
                    if (cp >= 0xD800 && cp <= 0xDBFF) {
                        if (!consumir("\\u")) falhar("par substituto incompleto");
                        unsigned baixo = hex4();
                        if (baixo < 0xDC00 || baixo > 0xDFFF) falhar("par substituto invalido");
                        cp = 0x10000 + ((cp - 0xD800) << 10) + (baixo - 0xDC00);
                    } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                        falhar("par substituto isolado");
                    }
                    utf8(cp, out);
                    break;
                }
                default: falhar("escape desconhecido");
            }
        }
    }

    Json numero() {
        size_t ini = pos_;
        bool inteiro = true;
        if (t_[pos_] == '-') ++pos_;
        if (pos_ >= t_.size()) falhar("numero incompleto");
        if (t_[pos_] == '0') {
            ++pos_;
        } else if (t_[pos_] >= '1' && t_[pos_] <= '9') {
            while (pos_ < t_.size() && t_[pos_] >= '0' && t_[pos_] <= '9') ++pos_;
        } else {
            falhar("numero invalido");
        }
        if (pos_ < t_.size() && t_[pos_] == '.') {
            inteiro = false;
            ++pos_;
            size_t d = pos_;
            while (pos_ < t_.size() && t_[pos_] >= '0' && t_[pos_] <= '9') ++pos_;
            if (pos_ == d) falhar("fracao sem digitos");
        }
        if (pos_ < t_.size() && (t_[pos_] == 'e' || t_[pos_] == 'E')) {
            inteiro = false;
            ++pos_;
            if (pos_ < t_.size() && (t_[pos_] == '+' || t_[pos_] == '-')) ++pos_;
            size_t d = pos_;
            while (pos_ < t_.size() && t_[pos_] >= '0' && t_[pos_] <= '9') ++pos_;
            if (pos_ == d) falhar("expoente sem digitos");
        }
        std::string tok = t_.substr(ini, pos_ - ini);
        if (inteiro) {
            errno = 0;
            long long v = std::strtoll(tok.c_str(), nullptr, 10);
            if (errno == 0) return Json::inteiro(v);
        }
        double d = std::strtod(tok.c_str(), nullptr);
        if (!std::isfinite(d)) falhar("numero fora do intervalo");
        return Json::real(d);
    }
};

}  // namespace

std::string Json::serializar() const {
    std::string out;
    escrever(*this, out);
    return out;
}

Json Json::analisar(const std::string& texto) {
    Analisador a(texto);
    return a.documento();
}

}  // namespace poo

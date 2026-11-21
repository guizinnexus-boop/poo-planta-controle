// Utilitarios minimos de teste (cabecalho, nao e um teste: o Make so descobre autorais_*.cpp).
#pragma once
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>

#define VERIFICAR(cond, msg)                                                                  \
    do {                                                                                      \
        if (!(cond)) {                                                                        \
            std::cerr << "FALHA: " << (msg) << " [" << __FILE__ << ":" << __LINE__ << "]\n"; \
            std::exit(1);                                                                     \
        }                                                                                     \
    } while (0)

#define VERIFICAR_IGUAL(obtido, esperado, msg) VERIFICAR((obtido) == (esperado), msg)
#define VERIFICAR_PROXIMO(obtido, esperado, msg) VERIFICAR(std::fabs((obtido) - (esperado)) < 1e-9, msg)
#define VERIFICAR_LANCA(expr, Tipo, msg)                 \
    do {                                                 \
        bool lancou_ = false;                            \
        try { (void)(expr); } catch (const Tipo&) { lancou_ = true; } \
        VERIFICAR(lancou_, msg);                         \
    } while (0)

// Diretorio temporario removido ao sair do escopo.
class DiretorioTemporario {
public:
    DiretorioTemporario() {
        std::string modelo = (std::filesystem::temp_directory_path() / "poo_teste_XXXXXX").string();
        char* r = mkdtemp(modelo.data());
        if (!r) { std::cerr << "mkdtemp falhou\n"; std::exit(1); }
        caminho_ = modelo;
    }
    ~DiretorioTemporario() { std::error_code ec; std::filesystem::remove_all(caminho_, ec); }
    const std::string& caminho() const { return caminho_; }
private:
    std::string caminho_;
};

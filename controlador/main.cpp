// CLI do controlador. Uso:
//   controlador --dados DIR [--politica liga-desliga|proporcional] [--uma-vez]
//               [--intervalo S] [--limite-obsoleto S]
// Codigos de saida: 0 ok; 1 erro de execucao; 2 entrada invalida/indisponivel (--uma-vez); 64 uso incorreto.
#include <chrono>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

#include "controlador.hpp"
#include "tempo.hpp"

namespace {

int uso() {
    std::cerr << "uso: controlador --dados DIR [--politica liga-desliga|proporcional] [--uma-vez] [--intervalo S] [--limite-obsoleto S]\n";
    return 64;
}

bool numero_positivo(const std::string& s, double& out) {
    char* fim = nullptr;
    out = std::strtod(s.c_str(), &fim);
    return fim != s.c_str() && *fim == '\0' && out >= 0.0;
}

}  // namespace

int main(int argc, char** argv) {
    std::string dados = std::getenv("POO_DADOS") ? std::getenv("POO_DADOS") : "dados";
    std::string politica = "proporcional";
    bool uma_vez = false;
    double intervalo = 1.0;
    double limite = 10.0;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto valor = [&](std::string& dest) { if (i + 1 >= argc) return false; dest = argv[++i]; return true; };
        std::string v;
        if (a == "--uma-vez") uma_vez = true;
        else if (a == "--dados") { if (!valor(dados)) return uso(); }
        else if (a == "--politica") { if (!valor(politica)) return uso(); }
        else if (a == "--intervalo") { if (!valor(v) || !numero_positivo(v, intervalo)) return uso(); }
        else if (a == "--limite-obsoleto") { if (!valor(v) || !numero_positivo(v, limite)) return uso(); }
        else return uso();
    }

    auto pol = poo::criar_politica(politica);
    if (!pol) {
        std::cerr << "politica desconhecida: " << politica << "\n";
        return uso();
    }
    poo::ConfigControlador cfg;
    cfg.limite_obsoleto_s = static_cast<long long>(limite);
    poo::Controlador ctl{poo::RepositorioArquivos(dados), std::move(pol), cfg};

    static const char* const NOMES[] = {"nova-aquisicao", "sessao-reiniciada", "ciclo-repetido", "ciclo-anterior", "indisponivel", "invalida"};
    int codigo = 0;
    do {
        try {
            poo::ResultadoCiclo r = ctl.executar(poo::agora_utc());
            std::cout << "[" << NOMES[static_cast<int>(r.situacao)] << "]";
            if (r.comando_emitido) std::cout << " comando id=" << r.id_comando;
            std::cout << " confirmacao=" << r.confirmacao << std::endl;
            for (const auto& d : r.diagnosticos) std::cerr << "  diag: " << d << "\n";
            if (!r.entrada_ok()) codigo = 2;
        } catch (const std::exception& e) {
            std::cerr << "erro: " << e.what() << "\n";
            codigo = 1;
        }
        if (uma_vez) break;
        std::this_thread::sleep_for(std::chrono::duration<double>(intervalo));
    } while (true);
    return uma_vez ? codigo : 0;
}

#include "repositorio.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace poo {
namespace fs = std::filesystem;

RepositorioArquivos::RepositorioArquivos(std::string diretorio) : diretorio_(std::move(diretorio)) {}

std::string RepositorioArquivos::caminho(const std::string& nome) const {
    return (fs::path(diretorio_) / nome).string();
}

RepositorioArquivos::Leitura RepositorioArquivos::ler(const std::string& nome) const {
    Leitura r;
    std::ifstream in(caminho(nome), std::ios::binary);
    if (!in) {
        r.status = Status::Ausente;
        return r;
    }
    std::ostringstream buf;
    buf << in.rdbuf();
    try {
        r.documento = Json::analisar(buf.str());
        r.status = Status::Ok;
    } catch (const ErroJson& e) {
        r.status = Status::Invalido;
        r.erro = e.what();
    }
    return r;
}

void RepositorioArquivos::gravar(const std::string& nome, const Json& documento) const {
    const std::string destino = caminho(nome);
    const std::string temporario = destino + ".tmp";
    {
        std::ofstream out(temporario, std::ios::binary | std::ios::trunc);
        if (!out) throw std::runtime_error("nao foi possivel criar " + temporario);
        out << documento.serializar() << '\n';
        out.flush();
        if (!out) {
            out.close();
            std::remove(temporario.c_str());
            throw std::runtime_error("falha ao escrever " + temporario);
        }
    }
    std::error_code ec;
    fs::rename(temporario, destino, ec);
    if (ec) {
        std::remove(temporario.c_str());
        throw std::runtime_error("falha ao substituir " + destino + ": " + ec.message());
    }
}

}  // namespace poo

// Testa RepositorioArquivos: leitura, ausencia, JSON invalido, escrita atomica.
#include <filesystem>
#include <fstream>

#include "../../controlador/repositorio.hpp"
#include "teste_util.hpp"

using namespace poo;

static void escrever_bruto(const std::string& caminho, const std::string& texto) {
    std::ofstream(caminho, std::ios::binary) << texto;
}

int main() {
    DiretorioTemporario tmp;
    RepositorioArquivos repo(tmp.caminho());

    // Comum: gravar e ler de volta.
    Json j = Json::objeto();
    j.definir("a", Json::inteiro(7));
    repo.gravar("x.json", j);
    auto l = repo.ler("x.json");
    VERIFICAR(l.status == RepositorioArquivos::Status::Ok, "leitura apos gravar");
    VERIFICAR_IGUAL(l.documento.buscar("a")->inteiro_valor(), 7LL, "conteudo gravado");
    VERIFICAR(!std::filesystem::exists(repo.caminho("x.json.tmp")), "temporario nao sobra");

    // Fronteira: substituir preserva um documento completo (nunca meio-termo).
    Json k = Json::objeto();
    k.definir("a", Json::inteiro(8));
    repo.gravar("x.json", k);
    VERIFICAR_IGUAL(repo.ler("x.json").documento.buscar("a")->inteiro_valor(), 8LL, "substituicao completa");

    // Falha: arquivo ausente.
    VERIFICAR(repo.ler("nao_existe.json").status == RepositorioArquivos::Status::Ausente, "ausente");

    // Falha: JSON invalido e arquivo vazio.
    escrever_bruto(repo.caminho("ruim.json"), "{\"a\": ");
    auto ruim = repo.ler("ruim.json");
    VERIFICAR(ruim.status == RepositorioArquivos::Status::Invalido && !ruim.erro.empty(), "JSON truncado e invalido com mensagem");
    escrever_bruto(repo.caminho("vazio.json"), "");
    VERIFICAR(repo.ler("vazio.json").status == RepositorioArquivos::Status::Invalido, "arquivo vazio e invalido");
    escrever_bruto(repo.caminho("nan.json"), "{\"v\": NaN}");
    VERIFICAR(repo.ler("nan.json").status == RepositorioArquivos::Status::Invalido, "NaN e invalido");

    // Falha de escrita: diretorio inexistente lanca e nao cria nada.
    RepositorioArquivos sem_dir(tmp.caminho() + "/nao/existe");
    VERIFICAR_LANCA(sem_dir.gravar("x.json", j), std::runtime_error, "gravar em diretorio inexistente lanca");

    // Estado preservado: apos falha de escrita, o destino anterior continua legivel.
    VERIFICAR_IGUAL(repo.ler("x.json").documento.buscar("a")->inteiro_valor(), 8LL, "destino anterior preservado");

    std::cout << "OK: RepositorioArquivos (comum, fronteira, falha, escrita atomica)\n";
    return 0;
}

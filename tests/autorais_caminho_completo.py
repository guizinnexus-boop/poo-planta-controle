"""Caminho completo: planta (CLI real) -> controlador C++ (compilado aqui) -> telemetria.json -> servico da tela.

Compila controlador/*.cpp (sem main de teste) num diretorio temporario; usa a planta do repositorio via
`python3 -m planta.simulador`. Defina CXX para escolher o compilador (padrao: g++).
"""
import glob
import json
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from datetime import datetime, timezone

from supervisorio.fonte import FonteTelemetria
from supervisorio.historico import RepositorioHistorico, exportar_csv
from supervisorio.servico import ServicoSupervisorio

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CXX = os.environ.get("CXX", "g++")


@unittest.skipUnless(shutil.which(CXX), f"compilador {CXX} nao encontrado")
class CaminhoCompleto(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls._tmp = tempfile.TemporaryDirectory()
        cls.binario = os.path.join(cls._tmp.name, "controlador")
        fontes = sorted(glob.glob(os.path.join(RAIZ, "controlador", "*.cpp")))
        r = subprocess.run([CXX, "-std=c++17", "-O0", *fontes, "-o", cls.binario], capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError("falha ao compilar o controlador:\n" + r.stderr)

    @classmethod
    def tearDownClass(cls):
        cls._tmp.cleanup()

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.dados = os.path.join(self.tmp.name, "dados")
        os.makedirs(self.dados)

    def _planta(self, passos=3):
        r = subprocess.run([sys.executable, "-m", "planta.simulador", "--dados", self.dados, "--passos", str(passos),
                            "--intervalo", "0", "--seed", "7"], cwd=RAIZ, capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)

    def _controlador(self, politica="proporcional"):
        return subprocess.run([self.binario, "--dados", self.dados, "--uma-vez", "--politica", politica],
                              capture_output=True, text=True)

    def _ler(self, nome):
        with open(os.path.join(self.dados, nome), encoding="utf-8") as f:
            return json.load(f)

    def _servico(self):
        repo = RepositorioHistorico(os.path.join(self.tmp.name, "h.sqlite"))
        return ServicoSupervisorio(FonteTelemetria(self.dados), repo, limite_obsoleto_s=60,
                                   relogio=lambda: datetime.now(timezone.utc))

    def test_planta_ate_a_tela(self):
        self._planta(3)
        planta = self._ler("planta.json")
        r = self._controlador()
        self.assertEqual(r.returncode, 0, r.stderr)

        comando = self._ler("comando_planta.json")
        self.assertEqual(comando["sessao"], planta["sessao"])
        self.assertEqual(comando["id"], planta["ultimo_comando"] + 1)
        self.assertEqual(set(comando["atuadores"]), {"bomba", "aquecedor"})

        servico = self._servico()
        estado = servico.atualizar()
        self.assertEqual(estado.situacao, "ok")
        t = estado.telemetria
        # Telemetria preserva a origem da aquisicao e acrescenta alarmes.
        for campo in ("versao", "sessao", "ciclo", "timestamp", "leituras", "atuadores", "ultimo_comando", "erro_comando", "motivo_seguro"):
            self.assertEqual(t[campo], planta[campo], campo)
        self.assertIn("alarmes", t)
        self.assertEqual(t["controlador"]["solicitacao"]["id"], comando["id"])
        # A tela recebe a idade da aquisicao e o registro persistido.
        self.assertIsNotNone(estado.idade_s)
        self.assertEqual(servico.repo.contar(), 1)
        reg = servico.repo.consultar()[0]
        self.assertEqual((reg["sessao"], reg["ciclo"]), (planta["sessao"], planta["ciclo"]))
        valores = {l["tag"]: l["valor"] for l in planta["leituras"]}
        self.assertEqual(reg["nivel"], valores["LT-01"])
        self.assertEqual(reg["temperatura"], valores["TT-01"])
        self.assertIn(str(planta["ciclo"]), exportar_csv(servico.repo.consultar()).splitlines()[1])

    def test_reler_o_mesmo_ciclo_nao_gera_comando_novo_nem_registro_novo(self):
        self._planta(2)
        self._controlador()
        comando = self._ler("comando_planta.json")
        r = self._controlador()
        self.assertEqual(r.returncode, 0)
        self.assertIn("ciclo-repetido", r.stdout)
        self.assertEqual(self._ler("comando_planta.json"), comando)
        servico = self._servico()
        servico.atualizar()
        self.assertFalse(servico.atualizar().inserido)
        self.assertEqual(servico.repo.contar(), 1)

    def test_planta_reiniciada_gera_nova_sessao_e_novo_registro(self):
        self._planta(2)
        self._controlador()
        servico = self._servico()
        servico.atualizar()
        sessao1 = self._ler("planta.json")["sessao"]
        self._planta(2)  # novo processo da planta => nova sessao
        self.assertNotEqual(self._ler("planta.json")["sessao"], sessao1)
        r = self._controlador()
        self.assertIn("sessao-reiniciada", r.stdout)
        self.assertTrue(servico.atualizar().inserido)
        self.assertEqual(len(servico.repo.sessoes()), 2)

    def test_falha_planta_json_invalido_retorna_nao_zero_e_preserva_telemetria(self):
        self._planta(2)
        self._controlador()
        boa = self._ler("telemetria.json")
        with open(os.path.join(self.dados, "planta.json"), "w") as f:
            f.write("{truncado")
        r = self._controlador()
        self.assertEqual(r.returncode, 2)
        self.assertIn("AQUISICAO_INDISPONIVEL", r.stderr)
        self.assertEqual(self._ler("telemetria.json"), boa)

    def test_cli_politica_desconhecida_e_uso_incorreto(self):
        self.assertEqual(subprocess.run([self.binario, "--politica", "x", "--uma-vez"], capture_output=True).returncode, 64)
        self.assertEqual(subprocess.run([self.binario, "--nao-existe"], capture_output=True).returncode, 64)


if __name__ == "__main__":
    unittest.main()

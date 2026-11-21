"""Testes da persistencia SQLite (Repository) e da exportacao CSV."""
import csv
import io
import os
import sqlite3
import tempfile
import unittest

from dados_teste import telemetria
from supervisorio.historico import COLUNAS, RepositorioHistorico, exportar_csv, linha_da_telemetria


class Historico(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.banco = os.path.join(self.tmp.name, "h.sqlite")
        self.repo = RepositorioHistorico(self.banco)

    def test_comum_registrar_e_consultar_em_ordem(self):
        for c in (1, 2, 3):
            self.assertTrue(self.repo.registrar(telemetria(ciclo=c, nivel=40.0 + c)))
        regs = self.repo.consultar()
        self.assertEqual([r["ciclo"] for r in regs], [1, 2, 3])
        self.assertEqual(regs[2]["nivel"], 43.0)
        self.assertEqual(self.repo.contar(), 3)

    def test_nao_duplica_mesma_sessao_e_ciclo(self):
        self.assertTrue(self.repo.registrar(telemetria(ciclo=1)))
        self.assertFalse(self.repo.registrar(telemetria(ciclo=1)))  # rerun da tela
        self.assertFalse(self.repo.registrar(telemetria(ciclo=1, nivel=99.0)))  # mesma identidade
        self.assertEqual(self.repo.contar(), 1)
        self.assertEqual(self.repo.consultar()[0]["nivel"], 50.0)  # primeiro registro preservado

    def test_mesmo_ciclo_em_sessoes_diferentes_sao_registros_distintos(self):
        self.assertTrue(self.repo.registrar(telemetria(sessao="a", ciclo=1)))
        self.assertTrue(self.repo.registrar(telemetria(sessao="b", ciclo=1)))
        self.assertEqual(self.repo.contar(), 2)
        self.assertEqual(self.repo.sessoes(), ["a", "b"])
        self.assertEqual(len(self.repo.consultar(sessao="b")), 1)

    def test_persiste_apos_reabrir_o_banco(self):
        self.repo.registrar(telemetria(ciclo=1))
        outro = RepositorioHistorico(self.banco)  # "reiniciar a tela"
        self.assertEqual(outro.contar(), 1)
        self.assertFalse(outro.registrar(telemetria(ciclo=1)))
        self.assertEqual(outro.contar(), 1)

    def test_fronteira_limite_devolve_os_mais_recentes_em_ordem(self):
        for c in range(1, 6):
            self.repo.registrar(telemetria(ciclo=c))
        self.assertEqual([r["ciclo"] for r in self.repo.consultar(limite=2)], [4, 5])
        self.assertEqual(self.repo.ultimo()["ciclo"], 5)

    def test_fronteira_banco_vazio(self):
        self.assertEqual(self.repo.consultar(), [])
        self.assertIsNone(self.repo.ultimo())
        self.assertEqual(self.repo.contar(), 0)

    def test_falha_limite_invalido(self):
        with self.assertRaises(ValueError):
            self.repo.consultar(limite=0)

    def test_leitura_em_falha_vira_null_nao_valor_antigo(self):
        self.repo.registrar(telemetria(ciclo=1, temperatura=30.0))
        self.repo.registrar(telemetria(ciclo=2, status_temp="falha"))
        r = self.repo.consultar()
        self.assertEqual(r[0]["temperatura"], 30.0)
        self.assertIsNone(r[1]["temperatura"])
        self.assertEqual(r[1]["nivel"], 50.0)

    def test_alarmes_gravados_como_codigos(self):
        t = telemetria(alarmes=[{"codigo": "A", "mensagem": "m", "severidade": "aviso"},
                                {"codigo": "B", "mensagem": "m", "severidade": "erro"}])
        self.repo.registrar(t)
        self.assertEqual(self.repo.consultar()[0]["alarmes"], "A;B")

    def test_chave_primaria_no_banco(self):
        self.repo.registrar(telemetria())
        con = sqlite3.connect(self.banco)
        self.addCleanup(con.close)
        with self.assertRaises(sqlite3.IntegrityError):
            con.execute("INSERT INTO aquisicoes (sessao,ciclo,timestamp,bomba,aquecedor,ultimo_comando,alarmes) "
                        "VALUES ('s1',1,'x',0,0,0,'')")

    def test_linha_extrai_confirmacao_e_politica(self):
        linha = dict(zip(COLUNAS, linha_da_telemetria(telemetria(confirmacao="confirmado", politica="liga-desliga"))))
        self.assertEqual(linha["confirmacao"], "confirmado")
        self.assertEqual(linha["politica"], "liga-desliga")


class ExportarCsv(unittest.TestCase):
    def _ler(self, texto):
        return list(csv.DictReader(io.StringIO(texto)))

    def test_comum_csv_tem_exatamente_os_registros_consultados(self):
        with tempfile.TemporaryDirectory() as d:
            repo = RepositorioHistorico(os.path.join(d, "h.sqlite"))
            for c in (1, 2, 3):
                repo.registrar(telemetria(ciclo=c, nivel=10.0 * c))
            consultados = repo.consultar(limite=2)
            linhas = self._ler(exportar_csv(consultados))
        self.assertEqual([l["ciclo"] for l in linhas], ["2", "3"])
        self.assertEqual(linhas[1]["nivel"], "30.0")
        self.assertEqual(list(linhas[0].keys()), COLUNAS)

    def test_fronteira_sem_registros_exporta_so_cabecalho(self):
        texto = exportar_csv([])
        self.assertEqual(texto.strip().split(","), COLUNAS)
        self.assertEqual(self._ler(texto), [])

    def test_valor_nulo_vira_celula_vazia(self):
        with tempfile.TemporaryDirectory() as d:
            repo = RepositorioHistorico(os.path.join(d, "h.sqlite"))
            repo.registrar(telemetria(status_temp="falha"))
            linhas = self._ler(exportar_csv(repo.consultar()))
        self.assertEqual(linhas[0]["temperatura"], "")


if __name__ == "__main__":
    unittest.main()

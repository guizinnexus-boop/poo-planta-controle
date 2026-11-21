"""Testes do servico do supervisorio (logica da tela, sem Streamlit): fonte, idade, obsolescencia e recuperacao."""
import os
import tempfile
import unittest
from datetime import datetime, timedelta, timezone

from dados_teste import TS, gravar, telemetria
from supervisorio.fonte import FonteTelemetria
from supervisorio.historico import RepositorioHistorico
from supervisorio.servico import ServicoSupervisorio, resumo_leituras, series_para_grafico

T0 = datetime(2026, 1, 1, 10, 0, 0, tzinfo=timezone.utc)


class Servico(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.d = self.tmp.name
        self.agora = T0 + timedelta(seconds=3)
        self.repo = RepositorioHistorico(os.path.join(self.d, "h.sqlite"))
        self.srv = ServicoSupervisorio(FonteTelemetria(self.d), self.repo, limite_obsoleto_s=10, relogio=lambda: self.agora)

    def test_comum_telemetria_valida_mostra_idade_e_registra(self):
        gravar(self.d, telemetria(ciclo=1))
        e = self.srv.atualizar()
        self.assertEqual(e.situacao, "ok")
        self.assertEqual(e.idade_s, 3.0)
        self.assertFalse(e.obsoleta)
        self.assertTrue(e.inserido)
        self.assertEqual(self.repo.contar(), 1)

    def test_rerun_nao_duplica(self):
        gravar(self.d, telemetria(ciclo=1))
        self.srv.atualizar()
        e = self.srv.atualizar()
        self.srv.atualizar()
        self.assertFalse(e.inserido)
        self.assertEqual(self.repo.contar(), 1)

    def test_fronteira_idade_igual_ao_limite_nao_e_obsoleta(self):
        gravar(self.d, telemetria())
        self.agora = T0 + timedelta(seconds=10)
        self.assertFalse(self.srv.atualizar().obsoleta)
        self.agora = T0 + timedelta(seconds=10.5)
        e = self.srv.atualizar()
        self.assertTrue(e.obsoleta)
        self.assertEqual(e.situacao, "obsoleta")

    def test_obsoleta_continua_legivel_mas_sinalizada(self):
        gravar(self.d, telemetria())
        self.agora = T0 + timedelta(hours=1)
        e = self.srv.atualizar()
        self.assertIsNotNone(e.telemetria)
        self.assertTrue(e.obsoleta)
        self.assertEqual(e.idade_s, 3600.0)

    def test_falha_arquivo_ausente(self):
        e = self.srv.atualizar()
        self.assertEqual(e.situacao, "ausente")
        self.assertIsNone(e.telemetria)
        self.assertTrue(e.erros)
        self.assertEqual(self.repo.contar(), 0)

    def test_falha_json_invalido_preserva_historico_e_recupera(self):
        gravar(self.d, telemetria(ciclo=1, nivel=42.0))
        self.srv.atualizar()
        gravar(self.d, '{"versao": 1, "ses')  # truncado
        e = self.srv.atualizar()
        self.assertEqual(e.situacao, "invalida")
        self.assertIsNone(e.telemetria)  # nao apresenta dado inventado como atual
        self.assertEqual(e.ultimo_historico["ciclo"], 1)  # ultimo valido, identificado como historico
        self.assertEqual(self.repo.contar(), 1)
        gravar(self.d, telemetria(ciclo=2))  # recuperacao
        e = self.srv.atualizar()
        self.assertEqual(e.situacao, "ok")
        self.assertEqual(self.repo.contar(), 2)

    def test_falha_nan_e_contrato_violado_nao_entram_no_historico(self):
        gravar(self.d, '{"versao": 1, "valor": NaN}')
        self.assertEqual(self.srv.atualizar().situacao, "invalida")
        gravar(self.d, telemetria(bomba=500))
        self.assertEqual(self.srv.atualizar().situacao, "invalida")
        self.assertEqual(self.repo.contar(), 0)

    def test_reinicio_de_sessao_cria_registros_novos(self):
        gravar(self.d, telemetria(sessao="a", ciclo=7))
        self.srv.atualizar()
        gravar(self.d, telemetria(sessao="b", ciclo=1))
        self.assertTrue(self.srv.atualizar().inserido)
        self.assertEqual(self.repo.sessoes(), ["a", "b"])

    def test_limite_invalido_rejeitado(self):
        with self.assertRaises(ValueError):
            ServicoSupervisorio(FonteTelemetria(self.d), self.repo, limite_obsoleto_s=0)

    def test_servico_nao_escreve_na_pasta_de_dados_da_planta(self):
        gravar(self.d, telemetria())
        self.srv.atualizar()
        self.assertFalse(os.path.exists(os.path.join(self.d, "comando_planta.json")))
        self.assertFalse(os.path.exists(os.path.join(self.d, "planta.json")))


class Apresentacao(unittest.TestCase):
    def test_series_deixam_lacuna_quando_leitura_invalida(self):
        regs = [{"ciclo": 1, "nivel": 10.0, "temperatura": 20.0}, {"ciclo": 2, "nivel": 11.0, "temperatura": None}]
        s = series_para_grafico(regs)
        self.assertEqual(s["Nivel LT-01 (%)"], [10.0, 11.0])
        self.assertEqual(s["Temperatura TT-01 (C)"], [20.0, None])
        self.assertEqual(s["ciclo"], [1, 2])

    def test_series_vazias(self):
        self.assertEqual(series_para_grafico([])["ciclo"], [])

    def test_resumo_leituras_com_unidade_e_falha(self):
        r = resumo_leituras(telemetria(status_temp="falha"))
        self.assertEqual(r[0], {"tag": "LT-01", "valor": 50.0, "unidade": "%", "status": "operando"})
        self.assertIsNone(r[1]["valor"])
        self.assertEqual(r[1]["status"], "falha")
        self.assertEqual(r[1]["unidade"], "C")


if __name__ == "__main__":
    unittest.main()

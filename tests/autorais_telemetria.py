"""Testes do dominio de telemetria: validacao, idade e leitura de valores."""
import unittest
from datetime import datetime, timezone

from dados_teste import com, telemetria
from supervisorio.telemetria import analisar_timestamp, idade_segundos, valor_da_leitura, validar_telemetria

T0 = datetime(2026, 1, 1, 10, 0, 0, tzinfo=timezone.utc)


class ValidarTelemetria(unittest.TestCase):
    def test_comum_documento_valido_nao_tem_erros(self):
        self.assertEqual(validar_telemetria(telemetria()), [])

    def test_comum_alarme_bem_formado_aceito(self):
        t = telemetria(alarmes=[{"codigo": "NIVEL_BAIXO", "mensagem": "m", "severidade": "aviso"}])
        self.assertEqual(validar_telemetria(t), [])

    def test_fronteira_atuador_em_0_e_100_aceitos(self):
        self.assertEqual(validar_telemetria(telemetria(bomba=0, aquecedor=100)), [])

    def test_fronteira_atuador_fora_da_faixa_rejeitado(self):
        self.assertTrue(validar_telemetria(telemetria(bomba=100.01)))
        self.assertTrue(validar_telemetria(telemetria(aquecedor=-0.01)))

    def test_fronteira_ciclo_1_valido_e_0_invalido(self):
        self.assertEqual(validar_telemetria(telemetria(ciclo=1)), [])
        self.assertTrue(validar_telemetria(telemetria(ciclo=0)))

    def test_falha_booleano_nao_e_inteiro_nem_numero(self):
        self.assertTrue(validar_telemetria(com(telemetria(), versao=True)))
        self.assertTrue(validar_telemetria(com(telemetria(), ciclo=True)))
        self.assertTrue(validar_telemetria(telemetria(bomba=True)))

    def test_falha_campos_obrigatorios_e_tipos(self):
        base = telemetria()
        for campo in ("versao", "sessao", "ciclo", "timestamp", "leituras", "atuadores", "ultimo_comando",
                      "erro_comando", "motivo_seguro", "alarmes"):
            doc = dict(base)
            del doc[campo]
            self.assertTrue(validar_telemetria(doc), f"deveria rejeitar sem {campo}")
        self.assertTrue(validar_telemetria(com(base, versao=2)))
        self.assertTrue(validar_telemetria(com(base, sessao="")))
        self.assertTrue(validar_telemetria(com(base, alarmes=[{"codigo": "X"}])))
        self.assertTrue(validar_telemetria([]))

    def test_falha_valor_nan_ou_texto_na_leitura(self):
        t = telemetria()
        t["leituras"][0]["valor"] = float("nan")
        self.assertTrue(validar_telemetria(t))
        t["leituras"][0]["valor"] = "50"
        self.assertTrue(validar_telemetria(t))

    def test_comum_leitura_nula_com_falha_e_valida_no_contrato(self):
        self.assertEqual(validar_telemetria(telemetria(status_temp="falha")), [])

    def test_validacao_nao_altera_o_documento(self):
        t = telemetria()
        antes = repr(t)
        validar_telemetria(t)
        self.assertEqual(repr(t), antes)


class Timestamp(unittest.TestCase):
    def test_formato_exato(self):
        self.assertEqual(analisar_timestamp("2026-01-01T10:00:00Z"), T0)

    def test_rejeita_formatos_proximos(self):
        for ruim in ("2026-01-01 10:00:00Z", "2026-01-01T10:00:00", "2026-01-01T10:00:00+00:00",
                     "2026-02-30T10:00:00Z", "2026-01-01T24:00:00Z", None, 5):
            self.assertIsNone(analisar_timestamp(ruim), ruim)


class Idade(unittest.TestCase):
    def test_comum(self):
        agora = datetime(2026, 1, 1, 10, 0, 7, tzinfo=timezone.utc)
        self.assertEqual(idade_segundos("2026-01-01T10:00:00Z", agora), 7.0)

    def test_fronteira_idade_zero_e_relogio_adiantado_nao_fica_negativa(self):
        self.assertEqual(idade_segundos("2026-01-01T10:00:00Z", T0), 0.0)
        atras = datetime(2026, 1, 1, 9, 59, 0, tzinfo=timezone.utc)
        self.assertEqual(idade_segundos("2026-01-01T10:00:00Z", atras), 0.0)

    def test_falha_timestamp_invalido(self):
        self.assertIsNone(idade_segundos("lixo", T0))


class ValorDaLeitura(unittest.TestCase):
    def test_comum(self):
        t = telemetria(nivel=42.5, temperatura=31.0)
        self.assertEqual(valor_da_leitura(t, "LT-01"), 42.5)
        self.assertEqual(valor_da_leitura(t, "TT-01"), 31.0)

    def test_falha_leitura_em_falha_nunca_vira_valor(self):
        t = telemetria(status_temp="falha")
        self.assertIsNone(valor_da_leitura(t, "TT-01"))
        self.assertEqual(valor_da_leitura(t, "LT-01"), 50.0)  # grandezas independentes

    def test_falha_tag_ausente(self):
        self.assertIsNone(valor_da_leitura(telemetria(), "XX-99"))


if __name__ == "__main__":
    unittest.main()

"""Servico do supervisorio: junta fonte, validacao, historico e idade. Testavel sem tela."""
from __future__ import annotations

from dataclasses import dataclass, field
from datetime import datetime, timezone
from typing import Callable, Optional

from .fonte import FonteTelemetria
from .historico import RepositorioHistorico
from .telemetria import idade_segundos, valor_da_leitura, TAG_NIVEL, TAG_TEMPERATURA

LIMITE_OBSOLETO_PADRAO_S = 10.0  # mesmo valor padrao do controlador; ver README


@dataclass
class EstadoTela:
    situacao: str  # "ok" | "obsoleta" | "ausente" | "invalida"
    telemetria: Optional[dict] = None       # so quando valida (obsoleta ou nao)
    idade_s: Optional[float] = None
    obsoleta: bool = False
    erros: list[str] = field(default_factory=list)
    inserido: bool = False                  # esta atualizacao gravou um registro novo?
    ultimo_historico: Optional[dict] = None  # ultimo registro persistido, identificado como antigo


class ServicoSupervisorio:
    def __init__(self, fonte: FonteTelemetria, repo: RepositorioHistorico,
                 limite_obsoleto_s: float = LIMITE_OBSOLETO_PADRAO_S,
                 relogio: Callable[[], datetime] = lambda: datetime.now(timezone.utc)):
        if limite_obsoleto_s <= 0:
            raise ValueError("limite_obsoleto_s deve ser positivo")
        self.fonte = fonte
        self.repo = repo
        self.limite = limite_obsoleto_s
        self.relogio = relogio

    def atualizar(self) -> EstadoTela:
        """Le a telemetria, registra no historico (sem duplicar) e informa idade/obsolescencia."""
        res = self.fonte.ler()
        agora = self.relogio()
        if res.situacao != "ok":
            return EstadoTela(
                situacao="ausente" if res.situacao == "ausente" else "invalida",
                erros=res.erros, ultimo_historico=self.repo.ultimo())
        tele = res.telemetria
        inserido = self.repo.registrar(tele)
        idade = idade_segundos(tele["timestamp"], agora)
        obsoleta = idade is None or idade > self.limite
        return EstadoTela(
            situacao="obsoleta" if obsoleta else "ok", telemetria=tele, idade_s=idade,
            obsoleta=obsoleta, inserido=inserido)


def series_para_grafico(registros: list[dict]) -> dict[str, list]:
    """Series das duas grandezas. Valores ausentes (leitura invalida) ficam None: o grafico mostra lacuna."""
    return {
        "ciclo": [r["ciclo"] for r in registros],
        "Nivel LT-01 (%)": [r["nivel"] for r in registros],
        "Temperatura TT-01 (C)": [r["temperatura"] for r in registros],
    }


def resumo_leituras(tele: dict) -> list[dict]:
    """Leituras com unidade e status, para exibicao (valor None se nao operando)."""
    out = []
    for l in tele["leituras"]:
        ok = l.get("status") == "operando"
        out.append({"tag": l["tag"], "valor": valor_da_leitura(tele, l["tag"]) if ok else None,
                    "unidade": l["unidade"], "status": l["status"]})
    return out

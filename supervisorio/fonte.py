"""Fonte de dados: le dados/telemetria.json. O supervisorio nunca escreve na planta nem no controlador."""
from __future__ import annotations

import json
import os
from dataclasses import dataclass, field
from typing import Optional

from .telemetria import validar_telemetria

ARQUIVO_TELEMETRIA = "telemetria.json"


@dataclass
class ResultadoFonte:
    situacao: str  # "ok" | "ausente" | "invalido"
    telemetria: Optional[dict] = None
    erros: list[str] = field(default_factory=list)


class FonteTelemetria:
    def __init__(self, diretorio: str):
        self.caminho = os.path.join(diretorio, ARQUIVO_TELEMETRIA)

    def ler(self) -> ResultadoFonte:
        try:
            with open(self.caminho, encoding="utf-8") as f:
                doc = json.load(f, parse_constant=_rejeitar_constante)
        except FileNotFoundError:
            return ResultadoFonte("ausente", None, ["telemetria.json ausente"])
        except (json.JSONDecodeError, UnicodeDecodeError, ValueError) as e:
            return ResultadoFonte("invalido", None, [f"JSON invalido: {e}"])
        erros = validar_telemetria(doc)
        if erros:
            return ResultadoFonte("invalido", None, erros)
        return ResultadoFonte("ok", doc, [])


def _rejeitar_constante(nome: str):
    raise ValueError(f"constante nao permitida: {nome}")  # NaN, Infinity

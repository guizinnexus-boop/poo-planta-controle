"""Dominio: validacao e leitura da telemetria. Sem Streamlit, sem arquivo, sem banco."""
from __future__ import annotations

import math
from datetime import datetime, timezone
from typing import Any, Optional

TAG_NIVEL = "LT-01"
TAG_TEMPERATURA = "TT-01"
FORMATO_UTC = "%Y-%m-%dT%H:%M:%SZ"


def _inteiro(v: Any) -> bool:
    return isinstance(v, int) and not isinstance(v, bool)


def _numero(v: Any) -> bool:
    return isinstance(v, (int, float)) and not isinstance(v, bool) and math.isfinite(v)


def analisar_timestamp(texto: Any) -> Optional[datetime]:
    """Retorna datetime UTC ou None se o texto nao seguir exatamente AAAA-MM-DDTHH:MM:SSZ."""
    if not isinstance(texto, str) or len(texto) != 20:
        return None
    try:
        return datetime.strptime(texto, FORMATO_UTC).replace(tzinfo=timezone.utc)
    except ValueError:
        return None


def validar_telemetria(doc: Any) -> list[str]:
    """Lista de erros do contrato (vazia = valida). Nao altera o documento."""
    if not isinstance(doc, dict):
        return ["documento nao e um objeto"]
    erros: list[str] = []
    if not (_inteiro(doc.get("versao")) and doc["versao"] == 1):
        erros.append("versao deve ser o inteiro 1")
    if not (isinstance(doc.get("sessao"), str) and doc["sessao"]):
        erros.append("sessao deve ser texto nao vazio")
    if not (_inteiro(doc.get("ciclo")) and doc["ciclo"] > 0):
        erros.append("ciclo deve ser inteiro positivo")
    if analisar_timestamp(doc.get("timestamp")) is None:
        erros.append("timestamp deve ser UTC AAAA-MM-DDTHH:MM:SSZ")
    if not (_inteiro(doc.get("ultimo_comando")) and doc["ultimo_comando"] >= 0):
        erros.append("ultimo_comando deve ser inteiro >= 0")
    for campo in ("erro_comando", "motivo_seguro"):
        if campo not in doc or not (doc[campo] is None or isinstance(doc[campo], str)):
            erros.append(f"{campo} deve ser texto ou null")

    at = doc.get("atuadores")
    if not isinstance(at, dict):
        erros.append("atuadores deve ser objeto")
    else:
        for nome in ("bomba", "aquecedor"):
            v = at.get(nome)
            if not (_numero(v) and 0 <= v <= 100):
                erros.append(f"atuadores.{nome} deve ser numero em [0,100]")

    leituras = doc.get("leituras")
    if not isinstance(leituras, list):
        erros.append("leituras deve ser lista")
    else:
        for i, l in enumerate(leituras):
            if not isinstance(l, dict):
                erros.append(f"leituras[{i}] nao e objeto")
                continue
            for campo in ("tag", "tipo", "unidade", "status"):
                if not isinstance(l.get(campo), str):
                    erros.append(f"leituras[{i}].{campo} deve ser texto")
            v = l.get("valor")
            if not (v is None or _numero(v)):
                erros.append(f"leituras[{i}].valor deve ser numero finito ou null")

    alarmes = doc.get("alarmes")
    if not isinstance(alarmes, list):
        erros.append("alarmes deve ser lista")
    else:
        for i, a in enumerate(alarmes):
            if not (isinstance(a, dict) and all(isinstance(a.get(c), str) for c in ("codigo", "mensagem", "severidade"))):
                erros.append(f"alarmes[{i}] deve ter codigo, mensagem e severidade textuais")
    return erros


def idade_segundos(timestamp: str, agora: datetime) -> Optional[float]:
    """Idade da aquisicao em segundos (>= 0), ou None se o timestamp for invalido."""
    t = analisar_timestamp(timestamp)
    if t is None:
        return None
    return max(0.0, (agora - t).total_seconds())


def valor_da_leitura(doc: dict, tag: str) -> Optional[float]:
    """Valor da tag somente se a leitura esta 'operando' e numerica; senao None (nunca um valor inventado)."""
    for l in doc.get("leituras", []):
        if l.get("tag") == tag:
            if l.get("status") == "operando" and _numero(l.get("valor")):
                return float(l["valor"])
            return None
    return None

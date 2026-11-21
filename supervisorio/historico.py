"""Persistencia (Repository): historico em SQLite deduplicado por (sessao, ciclo) e exportacao CSV."""
from __future__ import annotations

import csv
import io
import json
import sqlite3
from contextlib import closing
from typing import Optional

from .telemetria import TAG_NIVEL, TAG_TEMPERATURA, valor_da_leitura

COLUNAS = [
    "sessao", "ciclo", "timestamp", "nivel", "temperatura", "bomba", "aquecedor",
    "ultimo_comando", "politica", "confirmacao", "alarmes", "erro_comando", "motivo_seguro",
]

_DDL = """
CREATE TABLE IF NOT EXISTS aquisicoes (
    sessao TEXT NOT NULL,
    ciclo INTEGER NOT NULL,
    timestamp TEXT NOT NULL,
    nivel REAL,
    temperatura REAL,
    bomba REAL NOT NULL,
    aquecedor REAL NOT NULL,
    ultimo_comando INTEGER NOT NULL,
    politica TEXT,
    confirmacao TEXT,
    alarmes TEXT NOT NULL,
    erro_comando TEXT,
    motivo_seguro TEXT,
    PRIMARY KEY (sessao, ciclo)
)
"""


def linha_da_telemetria(t: dict) -> tuple:
    """Extrai a linha a persistir de uma telemetria JA validada. Leitura invalida vira NULL."""
    ctl = t.get("controlador") if isinstance(t.get("controlador"), dict) else {}
    conf = ctl.get("confirmacao") if isinstance(ctl.get("confirmacao"), dict) else {}
    codigos = ";".join(a["codigo"] for a in t["alarmes"])
    return (
        t["sessao"], t["ciclo"], t["timestamp"],
        valor_da_leitura(t, TAG_NIVEL), valor_da_leitura(t, TAG_TEMPERATURA),
        float(t["atuadores"]["bomba"]), float(t["atuadores"]["aquecedor"]),
        t["ultimo_comando"], ctl.get("politica"), conf.get("estado"), codigos,
        t["erro_comando"], t["motivo_seguro"],
    )


class RepositorioHistorico:
    def __init__(self, caminho_banco: str):
        self.caminho = caminho_banco
        with self._conectar() as con:
            con.execute(_DDL)

    def _conectar(self):
        return closing_connection(self.caminho)

    def registrar(self, telemetria: dict) -> bool:
        """Insere se (sessao, ciclo) for novo. Retorna True se inseriu, False se ja existia."""
        linha = linha_da_telemetria(telemetria)
        with self._conectar() as con:
            cur = con.execute(
                f"INSERT OR IGNORE INTO aquisicoes ({', '.join(COLUNAS)}) VALUES ({', '.join('?' * len(COLUNAS))})", linha)
            return cur.rowcount == 1

    def contar(self) -> int:
        with self._conectar() as con:
            return con.execute("SELECT COUNT(*) FROM aquisicoes").fetchone()[0]

    def consultar(self, sessao: Optional[str] = None, limite: int = 500) -> list[dict]:
        """Registros mais recentes (no maximo `limite`), em ordem cronologica crescente."""
        if limite <= 0:
            raise ValueError("limite deve ser positivo")
        sql = f"SELECT {', '.join(COLUNAS)} FROM aquisicoes"
        params: list = []
        if sessao is not None:
            sql += " WHERE sessao = ?"
            params.append(sessao)
        sql += " ORDER BY rowid DESC LIMIT ?"
        params.append(limite)
        with self._conectar() as con:
            linhas = con.execute(sql, params).fetchall()
        return [dict(zip(COLUNAS, l)) for l in reversed(linhas)]

    def sessoes(self) -> list[str]:
        with self._conectar() as con:
            return [r[0] for r in con.execute("SELECT sessao FROM aquisicoes GROUP BY sessao ORDER BY MIN(rowid)")]

    def ultimo(self) -> Optional[dict]:
        r = self.consultar(limite=1)
        return r[0] if r else None


def exportar_csv(registros: list[dict]) -> str:
    """CSV com exatamente os registros consultados (cabecalho sempre presente)."""
    buf = io.StringIO()
    w = csv.DictWriter(buf, fieldnames=COLUNAS, lineterminator="\n")
    w.writeheader()
    for r in registros:
        w.writerow({c: ("" if r.get(c) is None else r.get(c)) for c in COLUNAS})
    return buf.getvalue()


class closing_connection:
    """Conexao SQLite com commit/rollback e fechamento garantidos."""

    def __init__(self, caminho: str):
        self._con = sqlite3.connect(caminho)

    def __enter__(self):
        self._con.__enter__()
        return self._con

    def __exit__(self, *exc):
        try:
            return self._con.__exit__(*exc)
        finally:
            self._con.close()

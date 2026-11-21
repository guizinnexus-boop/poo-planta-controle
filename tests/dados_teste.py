"""Fabrica de telemetrias de teste (valores escolhidos, sem aleatoriedade). Nao e um teste."""
import copy
import json
import os

TS = "2026-01-01T10:00:00Z"


def telemetria(sessao="s1", ciclo=1, timestamp=TS, nivel=50.0, temperatura=30.0, bomba=0.0, aquecedor=0.0,
               ultimo_comando=0, alarmes=None, status_nivel="operando", status_temp="operando",
               politica="proporcional", confirmacao="nenhum"):
    return {
        "versao": 1, "sessao": sessao, "ciclo": ciclo, "timestamp": timestamp,
        "leituras": [
            {"tag": "LT-01", "tipo": "nivel", "valor": nivel if status_nivel == "operando" else None, "unidade": "%", "status": status_nivel},
            {"tag": "TT-01", "tipo": "temperatura", "valor": temperatura if status_temp == "operando" else None, "unidade": "C", "status": status_temp},
        ],
        "atuadores": {"bomba": bomba, "aquecedor": aquecedor},
        "ultimo_comando": ultimo_comando, "erro_comando": None, "motivo_seguro": None,
        "alarmes": alarmes if alarmes is not None else [],
        "controlador": {"politica": politica, "aquisicao_obsoleta": False,
                        "solicitacao": {"id": ultimo_comando + 1, "atuadores": {"bomba": 10.0, "aquecedor": 5.0}, "motivo": "politica"},
                        "confirmacao": {"id": None, "estado": confirmacao}, "gerado_em": timestamp},
    }


def gravar(diretorio, doc, nome="telemetria.json"):
    with open(os.path.join(diretorio, nome), "w", encoding="utf-8") as f:
        f.write(doc if isinstance(doc, str) else json.dumps(doc))


def com(base, **mudancas):
    d = copy.deepcopy(base)
    d.update(mudancas)
    return d

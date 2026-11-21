"""Tela Streamlit (camada fina). Toda regra esta em servico.py, telemetria.py e historico.py.

Executar:  POO_DADOS=dados streamlit run supervisorio/app.py
"""
from __future__ import annotations

import os
import sys

import streamlit as st

# Permite `streamlit run supervisorio/app.py` a partir da raiz do repositorio.
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from supervisorio.fonte import FonteTelemetria  # noqa: E402
from supervisorio.historico import RepositorioHistorico, exportar_csv  # noqa: E402
from supervisorio.servico import ServicoSupervisorio, resumo_leituras, series_para_grafico  # noqa: E402

DADOS = os.environ.get("POO_DADOS", "dados")
BANCO = os.environ.get("POO_BANCO", os.path.join(DADOS, "historico.sqlite"))
LIMITE_OBSOLETO_S = float(os.environ.get("POO_LIMITE_OBSOLETO", "10"))


@st.cache_resource
def _servico() -> ServicoSupervisorio:
    os.makedirs(os.path.dirname(BANCO) or ".", exist_ok=True)
    return ServicoSupervisorio(FonteTelemetria(DADOS), RepositorioHistorico(BANCO), LIMITE_OBSOLETO_S)


def _painel(servico: ServicoSupervisorio, sessao_filtro: str | None, limite: int) -> None:
    estado = servico.atualizar()

    if estado.situacao in ("ausente", "invalida"):
        st.error("Telemetria indisponivel: " + "; ".join(estado.erros))
        if estado.ultimo_historico:
            u = estado.ultimo_historico
            st.warning(f"Ultimo registro valido (ANTIGO): sessao {u['sessao']}, ciclo {u['ciclo']}, {u['timestamp']}")
    else:
        t = estado.telemetria
        if estado.obsoleta:
            st.error(f"Aquisicao OBSOLETA: idade {estado.idade_s:.0f} s (limite {servico.limite:.0f} s). Valores abaixo NAO sao atuais.")
        c1, c2, c3 = st.columns(3)
        c1.metric("Sessao / ciclo", f"{t['sessao']} / {t['ciclo']}")
        c2.metric("Idade da aquisicao", f"{estado.idade_s:.0f} s" if estado.idade_s is not None else "?")
        ctl = t.get("controlador", {})
        c3.metric("Politica", ctl.get("politica", "?"))

        st.subheader("Leituras")
        cols = st.columns(len(t["leituras"]))
        for col, l in zip(cols, resumo_leituras(t)):
            col.metric(l["tag"], f"{l['valor']:.2f} {l['unidade']}" if l["valor"] is not None else f"sem leitura ({l['status']})")

        st.subheader("Atuacao")
        a1, a2 = st.columns(2)
        a1.metric("Bomba (observada)", f"{t['atuadores']['bomba']:.1f} %")
        a2.metric("Aquecedor (observado)", f"{t['atuadores']['aquecedor']:.1f} %")
        conf = ctl.get("confirmacao", {})
        st.write(f"Ultimo comando aceito pela planta: **{t['ultimo_comando']}** | confirmacao do controlador: **{conf.get('estado', '?')}** (id {conf.get('id')})")
        sol = ctl.get("solicitacao")
        if sol:
            st.caption(f"Solicitacao do controlador (ainda nao observada): id {sol['id']}, bomba {sol['atuadores']['bomba']:.1f} %, aquecedor {sol['atuadores']['aquecedor']:.1f} % ({sol['motivo']})")

        st.subheader("Alarmes")
        if t["alarmes"]:
            for a in t["alarmes"]:
                (st.error if a["severidade"] == "erro" else st.warning)(f"{a['codigo']}: {a['mensagem']}")
        else:
            st.success("Sem alarmes ativos")

    st.subheader("Historico")
    registros = servico.repo.consultar(sessao=sessao_filtro, limite=limite)
    if registros:
        series = series_para_grafico(registros)
        st.caption("Nivel LT-01 (%)")
        st.line_chart({"Nivel": series["Nivel LT-01 (%)"]})
        st.caption("Temperatura TT-01 (C)")
        st.line_chart({"Temperatura": series["Temperatura TT-01 (C)"]})
        st.dataframe(registros, use_container_width=True)
        st.download_button("Exportar CSV (dados consultados)", exportar_csv(registros), file_name="historico.csv", mime="text/csv")
    else:
        st.info("Historico vazio.")


def main() -> None:
    st.set_page_config(page_title="Supervisorio", layout="wide")
    st.title("Supervisorio — planta, controlador e historico")
    servico = _servico()
    sessoes = servico.repo.sessoes()
    escolha = st.sidebar.selectbox("Sessao", ["(todas)"] + sessoes[::-1])
    limite = st.sidebar.number_input("Registros consultados", min_value=1, max_value=5000, value=200)
    auto = st.sidebar.checkbox("Atualizar automaticamente (2 s)", value=True)
    filtro = None if escolha == "(todas)" else escolha

    if auto:
        st.fragment(run_every=2)(lambda: _painel(servico, filtro, int(limite)))()
    else:
        if st.button("Atualizar"):
            pass
        _painel(servico, filtro, int(limite))


main()

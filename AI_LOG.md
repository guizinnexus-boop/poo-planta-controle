# AI_LOG.md

Registro de uso de IA. **A dupla deve revisar, completar a coluna "aceito/rejeitado" e reproduzir a validação**; cada integrante precisa explicar 100 % do código.

| Campo | Registro |
|---|---|
| Data | 2026-11-21 |
| Ferramenta | Claude (Anthropic), chat claude.ai, com ambiente de execução (g++ 13.3, Python 3.12; sem rede, sem Streamlit, sem `coverage`) |
| Pedido | "gerar o trabalho usando o conteúdo do AGENTS3.md" — escolhida a opção "tudo de uma vez": controlador C++, supervisório Streamlit/SQLite/CSV, testes autorais, README, matriz e este log |
| Gerado | `controlador/` (JSON próprio, tempo, aquisição, política, comando, alarmes, repositório, controlador, main); `supervisorio/`; `tests/cpp/autorais_*.cpp`; `tests/autorais_*.py`; `autoral.mk`; `README.md` |
| Aceito / rejeitado | **A preencher pela dupla.** Decisões da IA a avaliar: analisador JSON próprio em vez de `nlohmann/json` (indisponível sem rede); política padrão `proporcional`; limite de obsolescência 10 s; campo extra `controlador` na telemetria; atuadores assumidos como `{bomba, aquecedor}` |
| Justificativa técnica | ver README seções 4, 5 e 10 |
| Validação feita pela IA | `autoral.mk test`: 5 executáveis C++ e 50 testes Python OK; 6 mutações manuais detectadas pelos testes; execução contínua planta+controlador com planta **substituta** escrita pela IA a partir do AGENTS3.md (removida da entrega) |
| Não validado | planta real, Makefile/CI do repositório, `make cobertura`, execução do Streamlit (`app.py`), revisão cruzada |
| Commits | a preencher |

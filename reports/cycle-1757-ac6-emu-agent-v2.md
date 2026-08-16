# AC6 `emu-agent/v2` — reçu MCP cycle 1757

État : surface MCP v2 scellée en mode diagnostic strictement fail-closed. La
cible PAL exacte est `ac6-demo-xbox360-pal`, `Default.xex`, SHA-256
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.

Le backend est sélectionnable comme `demo-recomp` ou `demo-native`. Les six
outils v2 sont `emu_open_session`, `emu_step`, `emu_observe`, `emu_run_until`,
`emu_replay` et `emu_close_session`. La compatibilité v1 conserve les six
outils macro : `emu_capabilities`, `emu_run_episode`, `emu_compare_episodes`,
`emu_branch_episode`, `emu_minimize_reproducer` et `emu_inspect_artifact`.

Les contrats action, observation et reçu d’épisode sont respectivement
`ac6-agent-action/v1`, `ac6-agent-observation/v1` et
`ac6-emu-episode-receipt/v1`, sous enveloppe `emu-agent/v2`. L’ownership est
par session; les actions, observations et snapshots de reçu sont bornés,
immutables pour replay, et `close` est idempotent.

Le transport MCP stdio est présent pour le contrôle; en revanche aucun
transport emulator/backend n’est présent, aucun subprocess n’est lancé et
aucun socket runtime n’est possédé. Ainsi `open`, `step` et `run_until`
retournent `backend_unavailable`; `observe` ne produit que des domaines
`unavailable` avec valeur nulle. `QUALIFIED_PAL_ARTIFACT_SHA256S` est vide.
Il n’y a aucune observation `available` et aucun claim frontend.

Validation : `TMPDIR=/fastdata/lavaulta/tmp python -m pytest -q
tests/test_emu_agent_ac6.py` → **17 passed**. Revue : aucun constat. Les
hashes source et le hash canonique du reçu sont dans le JSON associé.

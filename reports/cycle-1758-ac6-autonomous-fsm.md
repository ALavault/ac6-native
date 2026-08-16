# AC6 autonomous FSM — checkpoint cycle 1758

La FSM autonome AC6 est implémentée en mode diagnostic borné, mais reste
explicitement `supported=false`. L’identité attendue est la démo PAL
`ac6-demo-xbox360-pal`, `Default.xex`, SHA-256
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.
Cette correspondance est vérifiée contre les constantes du protocole; elle ne
constitue pas un nouveau run PAL.

La configuration est `config/ac6-agent-fsm-v1.json` et le contrôleur est
`tools/emu_agent/controller/ac6_fsm.py`. Le graphe comporte 10 états, dont 9
états actifs budgétés; `terminal` porte séparément l’outcome `succeeded` ou
`failed`. Les budgets, `max_steps=240`, délais press/confirm/burst et la
commande fire sont déterministes.

Le contrôle de navigation est un P déterministe (`kp=0.25`, sortie bornée à
9000), avec saturation int16. Le burst d’attaque est limité à un frame. Toute
erreur d’observation, de session/tick/sequence, d’ACK `emu_step`, de domaine
requis ou de backend ferme la FSM et libère en neutre lorsque nécessaire.

La frontière d’exécution est stricte : allowlist SHA-256 PAL vide, aucun run
PAL, aucun succès mission, aucun transport emulator, subprocess ou socket.
Le transport de contrôle MCP stdio existe, mais ne vaut pas preuve d’exécution
émulateur. Les fixtures qualifiées des tests sont explicitement patchées et ne
promouvent pas `supported`.

Validation combinée :

```text
TMPDIR=/fastdata/lavaulta/tmp python -m pytest -q tests/test_emu_agent_ac6.py tests/test_ac6_agent_fsm.py
43 passed
python -m py_compile tools/emu_agent/controller/ac6_fsm.py tests/test_ac6_agent_fsm.py
PASS
```

Les hash des fichiers sources, du reçu JSON et de ce résumé sont dans le JSON
associé.

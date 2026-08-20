# AC6 PAL démo — état du mode manager A/B, cycle 1779

Verdict : **MODE-STATE-IDENTICAL / VISUAL-NO-GO**, `supported=false`.

Les replays scellés neutral et START (appui `0x10` au tick 252) atteignent
600 ticks de façon déterministe. Avec `AC6_DEMO_WATCH_MODE_STATE=1`, stdout
et stderr sont byte-identiques entre les routes.

Le manager `0x18980000` est observé au tick 107, avec requête `0`. Son mode
actif est `0x2E7E0080` (vtable `0x8201130C`) au tick 222 ; son état interne
passe de `0` à `1` au tick 266, sans modifier la requête manager. START ne
cause donc aucune transition guest persistante.

Cette observation ferme uniquement la frontière de contrôle-flow : le
renderer reste sans writeback guest et sans readback non noir. Elle ne valide
ni menu, ni mission, ni terminal. Les digests sont dans
[`ac6-demo-mode-state-ab-v1.json`](../analysis/demo/ac6-demo-mode-state-ab-v1.json).

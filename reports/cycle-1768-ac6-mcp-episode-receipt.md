# AC6 MCP v2 — reçu d’épisode cycle 1768

Verdict : **RECEIPT-CONTRACT-GO / RUNTIME-NO-GO**, `supported=false`.

Le commit `36b6504c` complète `ac6-agent-episode-receipt/v1`. Chaque reçu
scelle maintenant l’identité PAL démo, les digests des actions et observations,
des références d’artefacts content-addressed bornées, une première divergence
nullable et une raison d’arrêt explicite. Les références n’autorisent aucun
chemin, accès mémoire ou contenu arbitraire.

La route `demo-native` réelle produit deux références mémoire (`actions` et
`observations`) et conserve toutes les observations de gameplay à
`unavailable`. Une référence malformée ou dépassant la limite est rejetée par
le validateur avant acceptation.

```text
MCP transport tests                         13 PASS, 1 skip
native binary MCP route                    PASS
malformed artifact reference               REJECTED
py_compile                                  PASS
```

Ce checkpoint ne crée pas de readback qualifié et n’ouvre ni scheduler guest,
simulation, renderer, frontend, mission ni terminal.

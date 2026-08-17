# Cycle 1640 — A/B Vulkan neutral/START et intégration de la provenance `rr`

## Résultat

Le reçu `rr` de l’IB principal reste la preuve démo PAL de référence :
`analysis/demo/ac6-demo-main-ib-rr-provenance-v1.json` et le gate
`reports/ac6-demo-local-rr-fidelity-gate-20260815.md` sont vérifiés avec
`rr` local commit `7352eb807ed75e3b51be85fa6a27f121235dbfb0`. Les writers
qualifiés sont `0x821B0D70` (`95 4B 00 04`) au début de l’IB,
`0x821BA01C` (`94 CA 00 04`) à sa fin et `0x821B9D24` (`7D 2A C1 2E`) pour
la publication `C0013F00, 1274A000, 00000BD5`. Aucune preuve retail n’est
fusionnée. Le PC/valeur/fonction du premier dword est qualifié ; son
LR/thread/tick restent explicitement inconnus dans le reçu borné. Le writer
de la dernière fenêtre et la publication conservent les LR/thread/tick
disponibles.

Le PM4 capturé est le même IB principal démo, `3029` dwords, SHA-256
`d121c8d8cf55bcb755fa558c4d54a9311f4520fa2e8bb5e34b25920f107358d6`. La
table Xenos locale traite désormais `0x0A02..0x0A05` avec les valeurs exactes
observées et reste transactionnelle : une divergence trap avant commit ; la
resynchronisation est interdite.

## A/B frais

Deux processus Vulkan headless ont été lancés depuis le même store de contenu,
avec le binaire codegen-ON SHA-256
`b939517a34baba2107715d19daccd5cb0d4fa831616488f34aca3e3dff915a8b`,
`SDL_AUDIODRIVER=dummy` et `xvfb-run -a`, jusqu’au tick 253. Le run neutral
n’injecte aucune entrée ; le run START injecte uniquement le masque `0x10` au
tick 252. Les reçus complets et leurs digests sont dans
`analysis/demo/ac6-demo-neutral-start-vulkan-ab-v1.json`.

| Champ | Neutral | START |
|---|---:|---:|
| RTPLY | `c5357c6d…b1c5794` | `4a7326d9…e25724` |
| rapport | `33b6c8b3…b5685a7` | `2d0c391b…b91a6cf` |
| retour contrôlé | 4 (`max_ticks`) | 4 (`max_ticks`) |
| shader loads / draws / present renderer | 5 / 26 / 1 | 5 / 26 / 1 |
| modules validés / pipelines graphiques | 4 / 2 | 4 / 2 |
| readback draw 640×360 | `0b150fd3…ec58366` | `0b150fd3…ec58366` |
| readback resolve linéaire 1280×720 | `0c660f2b…a4913a5f` | `0c660f2b…a4913a5f` |

Les sous-arbres `outcome`, `milestones`, `graphics` et `scheduler` des deux
rapports sont byte-identiques. Chaque run compte 116 notifications de
présentation ; `VD_SWAP` reste à l’adresse `0x1374A000`, format 6, tiled,
1280×720. Les jalons `frontend`, `mission` et `terminal` restent faux.

## Qualification

- `demo-qualified` : parsing PM4 sans resynchronisation, provenance des
  writers de l’IB, création des modules/pipelines atteints, readbacks noirs
  déterministes et resolve borné ;
- `demo-observed` : livraison de l’événement START au tick 252 ;
- `unknown` : pixels frontend non noirs, transition visuelle causale, mission
  et résultat terminal.

Le digest noir n’est pas promu comme frontend jouable. Aucun screencap n’est
produit et aucun fallback visuel n’est activé. Les microcodes, sorties de
traduction et actifs propriétaires restent hors du suivi.

## Validation et prochain checkpoint

La garde Python existante couvre le reçu `rr` et ses instructions PAL ; la
nouvelle capsule scelle les hashes A/B, les compteurs renderer et les deux
readbacks. Relancer CTest codegen-ON/OFF et les audits source/complexité après
l’ajout de la garde, puis instrumenter un état guest non nul avant de considérer
une progression START. Le prochain test doit conserver le même A/B neutral puis
START et trapper dès qu’un champ PM4, EDRAM ou resolve n’est plus qualifié.

# Pipeline agent vision/contrôle AC6

Date : 2026-08-16
Cible : démo PAL `Default.xex`
SHA-256 : `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`

## Résultat

Un moteur externe synchrone est disponible dans
`tools/ac6_vision_control.py`. Il fournit :

- observations/actions JSONL qualifiées par l'identité du XEX ;
- options manette bornées et état neutre fail-closed ;
- classification exacte par hashes/signatures et règles OCR ;
- politique UCB exploitation/exploration avec poids de nouveauté et risque ;
- compilation de prompts en options autorisées, bloquées ou forcées ;
- archive Go-Explore de cellules/checkpoints et restaurations périodiques ;
- budgets de décisions et de frames invitées ;
- reçus stricts `action_id`/frames avancées/checkpoint restauré ;
- trace append-only chaînée par SHA-256 ;
- compilation de la découverte en movie rejouable sans vision, modèle ou HID.

La configuration `config/ac6-agent-behavior-explore-menus-v1.json` définit une
campagne bornée d'exploration des menus. Les règles initiales OCR sont dans
`config/ac6-agent-vision-rules-v1.json`. Le protocole et les frontières de
preuve sont documentés dans `docs/ac6-vision-control-pipeline.md`.

## Validation

- 10/10 tests unitaires passent ;
- `py_compile` passe ;
- Ruff passe ;
- les deux configurations JSON passent `json.tool` ;
- smoke CLI : capacité bridge validée, observation `PRESS START` classée
  `title`, action déterministe produite, trace/archive/movie publiés ;
- le validateur rejette identité XEX incorrecte, capacité bridge manquante,
  hash invalide, trace altérée et reçu d'action absent ou incohérent.

Hashes du checkpoint :

| Fichier | SHA-256 |
|---|---|
| `tools/ac6_vision_control.py` | `a47b938f2a6bc3ff53a81151ad1deb6038af54a484732f74ab2359412f73dfb3` |
| `tools/tests/test_ac6_vision_control.py` | `9ef2b27a8c743df9ff22ef4a4c8f4f80a4ae10782236fc170557c7696f6bf61f` |
| comportement menus | `1cbc7857bc46a3c4dfc4ffbc90b088e91499b8bb9a2f9299d5910d35f74de271` |
| règles vision | `75030d798127566a0f9aac9fad41d26de0189ab0dde369131491e1bd9b0cb4cc` |
| documentation | `bcb5bb13dcaed9088d2eadc56b7dfda93ec3496d9c361127f29ea567613f65bf` |

## Risque résiduel

L'AppImage Xenia Edge `60ff861` ne contient aucune interface observable de
pause après `XE_SWAP`, frame-step exact, injection XAM et restauration de
checkpoint. Le moteur exige donc un manifeste
`ac6-xenia-agent-bridge-capabilities/v1` portant ces cinq capacités et refuse
de démarrer autrement. Aucun `sleep`, `SIGSTOP`, `xdotool` ou capture X11 ne
remplace cette frontière.

Le prochain checkpoint est limité au côté Xenia : exposer le bridge
`completed_xe_swap -> observation -> action -> exact guest step` depuis une
source Edge révisionnée, puis exécuter deux découvertes et deux replays frais
depuis le même checkpoint. Les images Xenia resteront oracle-only.

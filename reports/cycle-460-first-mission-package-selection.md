# AC6 cycle 460 — sélection du paquet de première mission

Date : 2026-08-02  
Cible : AC6 PAL, XEX SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`,
base image `0x82000000`.

## Résultat

La transition campagne ne chargeait pas le paquet de la première mission. À
l'appel PAL `0x8218F3A0`, `sub_8218F358` calcule l'entrée DATA comme
`209 + sub_820943B0(contexte_niveau)`. Le runtime natif publiait niveau `0`,
mode `1`, sélecteur `0`, donc l'entrée 209. Les entrées 190 et 209 sont des
wrappers SWG/NTXR et ne satisfont pas le contrat du constructeur de scène ;
l'entrée 210 est la première qui contient BRDB en membre 0 et BMAP en membre 1.

Un bridge borné au caller, au pointeur racine PAL et au triplet `(0,1,0)` fait
retourner `1` à `sub_820943B0`. Les autres états conservent le comportement
invité. Cette correction remplace l'état noir vivant par une scène cohérente :
un enregistrement de scène, un propriétaire de tâche et un payload de tâche
non nuls. La tâche de première mission est donc créée.

## Nouvelle frontière

La mission n'est pas encore jouable. En sautant l'introduction, la timeline
principale avance de la frame 1 à la frame 359. Tous les handlers qualifiés
entrent et retournent ; le compteur atteint zéro, puis le runtime reçoit un
SIGSEGV avec PC hôte nul. L'analyse de `sub_8237CC58` montre qu'après la boucle
principale il parcourt récursivement les timelines enfants depuis `state+228`,
avec l'appel invité à `0x8237D0FC`. L'attribution exacte du PC nul à l'un de ces
enfants reste **heuristique** ; la position du crash après la frame 359 et la
fin des handlers principaux est **dynamique**.

La prochaine tranche est donc bornée : identifier l'état enfant, son candidat,
son type et sa cible indirecte à l'appel `0x8237D0FC`, puis corriger le contrat
minimal qui rend cette cible valide. Le HUD, les contrôles et la condition de
jouabilité ne pourront être validés qu'après ce passage.

## Hypothèses invalidées

- Publier durablement le niveau `1` via `sub_82196508` produit le même crash ;
  cette mutation a été retirée.
- GDB perturbe les signaux récupérables et les gates de présentation du
  runtime ; les sondes exactes et les runs bornés restent la voie de preuve.
- Locale, A et Start avaient déjà été invalidés et n'ont pas été rejoués.

## Validation

- Tests AC6 ciblés : **6/6 pass**, action
  `5d1f23f99077560ec92d3d2cfa1de1dbed55d2df45968c4424261f78e7ffd8e0`.
- CTest complet : **1609 pass, 4 skip, 6 échecs de référence sur 1619**,
  identique au cycle 459 ; action
  `ee110a9c1a08fe45b912f6e0b497bd537c676e4351c48285e65c6785f4a91a82`.
- JUnit complet : `reports/logs/cycle-460-first-mission-full-ctest.xml`,
  SHA-256 brut
  `8683b600a0cbb64d5422229f5801afe84c7d210826b309c7c448a46262664c70`.
- Trace de la frontière :
  `reports/logs/cycle-460-first-mission-count-probe/ac6recomp.log`.
- Binaire natif validé SHA-256 :
  `dc92c3cc53425d41b3024bf68d7b5edd8400884e1b4d8a50b558296cd7751ad2`.
- `git diff --check` passe ; aucun processus AC6/Xvfb orphelin détecté.

## Changements

- `src/ac6_campaign_resource_bridge.h` : prédicat pur de sélection.
- `src/ac6_backend_fixes/ac6_campaign_resource_bridge_test.cpp` : cas positif
  et gardes négatives.
- `src/ac6_backend_fixes/ac6_ui_input_dispatch_probe.cpp` : bridge PAL exact et
  sondes de timeline activables par `ac6_log_ui_dispatch`.
- `scripts/ac6-*-probe.steps` et `scripts/ac6-first-mission-playability.steps` :
  routes de reproduction bornées.

Le mismatch global de résolution et le dialogue `PLEASE WAIT` affichant
l'atlas de police restent deux défauts visuels adjacents, indépendants de cette
frontière de gameplay.

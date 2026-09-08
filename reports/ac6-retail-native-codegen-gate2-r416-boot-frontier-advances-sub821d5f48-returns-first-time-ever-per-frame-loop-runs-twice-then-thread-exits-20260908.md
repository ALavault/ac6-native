# AC6 retail NTSC-U/J — r416 — la frontière de boot avance : `sub_821D5F48` retourne pour la PREMIÈRE FOIS de toute la campagne (r358-r415), le corps de boucle par image (`sub_821D7AE0`/`sub_821D7CD0`) s'exécute deux fois, PUIS le thread du point d'entrée se termine de lui-même — `presented_frames` reste `0`

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r415 (le fil r399-r415 était considéré clos). `reports/
handoff/CURRENT.json` nomme le critère de succès réel derrière toute
la campagne r358-r399 : **`sub_821D5F48` (premier appel du thread
principal, entré au boot) n'a jamais retourné, à travers seize
constats successifs de la sous-chaîne allocateur** ; **la boucle par
image (`sub_821D7AE0`/`sub_821D7CD0`) n'a jamais exécuté, à travers
toute la campagne**. Ce cycle vérifie directement ce critère sur le
binaire corrigé par r414.

## Établi

### `sub_821D5F48` retourne — la première fois de toute la campagne

`r425_boot_progress.gdb` (points d'observation sur l'entrée ET le
retour de `sub_821D5F48`, plus l'entrée de `sub_821D7AE0`/
`sub_821D7CD0`), **reproduit à l'identique sur deux lancements
indépendants** (fenêtre `120000ms` puis `15000ms`) :

```
sub_821D5F48 ENTERED  #1  (t=0.000)
sub_821D5F48 RETURNED #1  (t≈+1.9s)
sub_821D7AE0 (boucle par image A) ENTERED #1  (t≈+2.0s)
sub_821D7CD0 (boucle par image B) ENTERED #1  (t≈+2.1s)
sub_821D7AE0 (boucle par image A) ENTERED #2  (t≈+2.1s, quelques ms après #1)
sub_821D7CD0 (boucle par image B) ENTERED #2  (t≈+2.1s, quelques ms après #1)
```

Timing quasi identique entre les deux lancements (`+1.86s`/`+1.88s`
pour le retour de `sub_821D5F48`). **Ni `sub_821D5F48` n'avait jamais
retourné, ni `sub_821D7AE0`/`sub_821D7CD0` n'avaient jamais été
atteints, sur les seize constats précédents documentés par `reports/
handoff/CURRENT.json`** — c'est la première fois, dans toute cette
campagne, que le boot dépasse ce point.

### Mais le corps de boucle ne tourne que DEUX fois, puis le thread se termine de lui-même

Sur les DEUX lancements (fenêtres `120000ms` et `15000ms`), le nombre
total de déclenchements de `sub_821D7AE0`/`sub_821D7CD0` est
**exactement 2 chacun**, tous survenant dans la même fraction de
seconde juste après le retour de `sub_821D5F48` — PAS une boucle
soutenue (qui, à ~60 images/seconde, produirait des centaines de
déclenchements sur une fenêtre de 15 ou 120 secondes). Le journal
`gdb-stdout` confirme, dans les deux runs : `presented_frames=0
state=1` puis **`ac6recomp: generated entry terminated its own
thread`** — le thread du point d'entrée retourne (fin normale, pas un
crash observé par ce harnais) après seulement ces deux itérations, et
`presented_frames` reste à `0`.

## Ce que ceci établit

**Progrès net et vérifié** : le correctif de r414 (retour de valeur
dans `sub_821FA9E0`, tracé par r413) débloque le boot bien au-delà de
tout ce qui avait été observé depuis r358 — `sub_821D5F48` retourne,
le code qui contient `sub_821D7AE0`/`sub_821D7CD0` s'exécute. **Ce
n'est cependant PAS encore "la boucle par image tourne"** : deux
itérations puis un arrêt volontaire du thread (`terminated its own
thread`, pas un plantage) est un nouveau point d'arrêt, pas la
résolution complète de `presented_frames=0`. La frontière de la
campagne s'est déplacée, elle n'a pas disparu.

## Non établi

- **Pourquoi le thread se termine après exactement deux itérations**
  — non tracé. Candidats non vérifiés : une condition de sortie
  normale et attendue de CE thread particulier (auquel cas
  `sub_821D7AE0`/`sub_821D7CD0` ne sont peut-être pas "la" boucle de
  présentation principale mais un sous-système d'initialisation à
  deux passes), ou un nouvel échec qui interrompt prématurément une
  boucle censée continuer indéfiniment.
- **Si `sub_821D7AE0`/`sub_821D7CD0` sont réellement la boucle de
  présentation par image**, ou un sous-système différent portant un
  nom voisin dans `reports/handoff/CURRENT.json` — l'identification
  vient de ce fichier de handoff, pas d'une relecture du code source
  de ces deux fonctions ce cycle.
- **Le contenu exact de ce que fait le thread entre `sub_821D5F48`
  ENTERED et RETURNED** (~1.9 secondes) — non tracé, pourrait
  contenir sa propre activité significative (chargement d'assets,
  etc.).
- Aucun correctif proposé ni appliqué ce cycle.

## Décisions prises

- Vérifier la reproductibilité sur DEUX lancements indépendants avant
  de publier, malgré le résultat déjà très net au premier lancement —
  cohérent avec la discipline déjà appliquée pour r398/r415 (preuve
  déterministe, pas un coup de chance).
- Ne pas relire `sub_821D7AE0`/`sub_821D7CD0` ni deviner pourquoi le
  thread se termine sans capture supplémentaire — nommé pour le cycle
  suivant plutôt que conclu ici (précédent r1111/r1113).

## Gate

Aucune source de production éditée ce cycle (le binaire testé est
celui reconstruit par r414).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées dans le commit de ce cycle)
`ctest` relancé en entier.

## Named for r417

Tracer pourquoi le thread du point d'entrée se termine de lui-même
après exactement deux itérations de `sub_821D7AE0`/`sub_821D7CD0` —
lire le code PPC de ces deux fonctions et de leur appelant pour
déterminer s'il s'agit d'une sortie normale (sous-système à deux
passes) ou d'un nouveau blocage empêchant une boucle soutenue.

## Files

`recompilation/ace-combat-6-retail/artifacts/retail-us-native-r404-bucket17-writer/r425_boot_progress.gdb/.log`,
`r425b_boot_progress_short.log`.

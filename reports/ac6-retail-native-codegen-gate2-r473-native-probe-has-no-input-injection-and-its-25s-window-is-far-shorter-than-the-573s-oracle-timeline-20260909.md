# AC6 retail NTSC-U/J — r473 — pourquoi le contenu réel n'est toujours pas atteint côté natif : AUCUN mécanisme d'injection d'entrée n'existe dans le produit natif, et la fenêtre de sonde la plus longue jamais essayée (25 s) est très inférieure au minutage réel du chemin oracle (573 s+) pour atteindre `world=1`

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r472 : le registre à 320 entrées est installé et vérifié
sans régression, mais la sonde `--probe-entry` native reste
`non_black=0` — le but final de la campagne r454-r472 n'est pas
atteint. Ce cycle investigue pourquoi.

## Établi — aucun mécanisme d'injection d'entrée n'est câblé dans le produit natif

Recherche exhaustive (`grep -rl` sur `native/src`, `native/include`)
pour tout mécanisme d'injection/rejeu d'entrée synthétique (input
replay, movie XAM, etc.) : **zéro résultat**. Le seul outil
apparenté, `tools/ac6_controller_input_replay.py`, porte dans son
propre docstring la confirmation explicite : « targets the native
runtime without pretending that the native runtime has guest XAM
calls » — c'est un format de données PARTAGÉ entre les pistes Xenia
et oracle AC6_recomp, PAS un mécanisme consommé par le runtime natif
lui-même. `ac6recomp_main.cpp` (le point d'entrée de la sonde) ne
lit ni ne rejoue aucune entrée : le boot se déroule intégralement en
mode « ouvert », sans aucune interaction possible.

## Établi — la fenêtre de sonde la plus longue jamais essayée est bien trop courte, indépendamment de l'entrée

Recherche dans tous les rapports r454-r472 pour la valeur de
`AC6_NATIVE_PROBE_WINDOW_MS` la plus longue jamais utilisée :
**25000 (25 s), jamais dépassée**. Or le chemin oracle (r470/r471),
avec la séquence de confirmation de déploiement CORRECTEMENT
envoyée, a mis **573 s** (9 min 33 s) pour atteindre
`[ac6-visual-phase] world=1`. Une fenêtre de sonde de 25 s est donc
environ **23 fois trop courte** par rapport au minutage réel observé
côté oracle pour ce même contenu retail — indépendamment de la
question d'injection d'entrée, la fenêtre actuelle ne pourrait de
toute façon jamais atteindre le vol réel.

## Établi — le contenu actuellement sondé est totalement disjoint du nouveau registre

Relancé avec le registre à 320 entrées fraîchement installé (r472,
`ctest` 11/11 déjà vérifié) : `AC6_NATIVE_ALLOW_ENTRY_PROBE=1
AC6_NATIVE_VD_TRACE=1 AC6_NATIVE_CAPTURE=1
AC6_NATIVE_PROBE_WINDOW_MS=25000` :
```
86 vd draw (inchangé depuis r453, 2026-09-08)
8 × « no pinned variant matches this draw state » (inchangé en nature)
presented_frames=5 state=2
capture pixels=921600 non_black=0 distinct_colors=1
```
**Compte de replis identique à avant l'extension du registre** — les
86 tirages de cette fenêtre de 25 s (probablement écran-titre/logos,
cohérent avec une fenêtre si courte) n'exercent AUCUN des 49
nouvelles entrées ajoutées par r472. Ce n'est pas un échec du
registre étendu (correctement installé et vérifié par `ctest`) —
c'est la confirmation que rien dans cette fenêtre ne l'atteint
encore.

## Non établi

- **Ce que les 86 tirages représentent réellement** (écran-titre,
  logos, chargement) — non identifié précisément ce cycle, mais
  cohérent avec une fenêtre de 25 s selon le minutage oracle.
- **Le coût réel de construction d'un mécanisme d'injection
  d'entrée natif** — hors périmètre d'estimation ce cycle.
- **Si une fenêtre de sonde simplement plus longue (sans aucune
  entrée) suffirait** à atteindre un contenu différent, ou si le jeu
  reste bloqué sur un écran nécessitant une confirmation réelle même
  sans limite de temps (l'équivalent natif de la boîte de dialogue
  « Deploy with this selection? » que r470 a dû corriger côté
  oracle) — non testé.

## Décisions prises

- **Ne pas construire de mécanisme d'injection d'entrée natif ce
  cycle** — un vrai morceau d'infrastructure (rejeu d'entrée XAM
  guest, synchronisé à la frame), pas un petit correctif de câblage,
  conformément à la discipline de portée de ce cycle (« un gain
  partiel caractérisé avec précision vaut mieux qu'une construction
  large non vérifiée »).
- **Ne pas essayer une fenêtre de sonde plus longue ce cycle** — sans
  mécanisme d'entrée, même une fenêtre de plusieurs minutes ne
  dépasserait probablement pas un écran nécessitant une confirmation
  (par analogie directe avec le comportement du chemin oracle avant
  le correctif r470) ; essayer sans comprendre d'abord le contenu
  actuel risquerait de consommer du temps de sonde sans preuve
  nouvelle.

## Gate

Aucune source de production modifiée ce cycle.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
`ctest` racine inchangé (aucune source de production modifiée ce
cycle) ; `ctest` natif reconfirmé 11/11 lors de la relance de la
sonde.

## Named for r474

Deux pistes distinctes, ni l'une ni l'autre bloquante, toutes deux du
vrai travail d'infrastructure plutôt qu'un correctif ponctuel : (1)
identifier précisément ce que les 86 tirages actuels représentent
(instrumenter/tracer le contenu de la fenêtre de 25 s pour confirmer
qu'il s'agit bien d'écran-titre/logos et non d'un autre blocage) ;
(2) si un mécanisme d'entrée s'avère nécessaire, dimensionner
concrètement sa construction (format d'entrée à consommer, point
d'injection dans le pipeline guest XAM, synchronisation de frame)
avant de l'entreprendre — décision d'investissement, pas à prendre
à la légère vu l'ampleur. Le registre à 320 entrées (r472) reste
correctement installé et vérifié ; cette découverte n'invalide rien
de son travail, elle explique seulement pourquoi son effet n'est pas
encore visible.

## Files

Aucun artefact gitignoré nouveau conservé (journal de sonde sous
`/fastdata/lavaulta/tmp/`, non conservé).

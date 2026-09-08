# AC6 retail NTSC-U/J — r402 — r401's microexec traces confirmed to execute real allocator logic (not a bail-out), concrete heap writes captured; semantic meaning of the written fields not yet resolved

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Un reviewer (avant publication de r401) a soulevé un doute valide : le
harnais microexec de r401 n'avait stubé que
`RtlEnterCriticalSection`/`RtlLeaveCriticalSection`, pas
`KeGetCurrentProcessType`/`KeBugCheckEx` (deux imports host aussi
présents dans le prologue de `sub_821F9E10`, à `0x823D03FC`/`0x823D03EC`).
Avec `callee_entries=0` et seulement 212/127 pas, l'hypothèse que la trace
ait heurté un de ces imports non stubés (retournant une valeur hôte
arbitraire) et bifurqué vers un chemin d'erreur plutôt que la logique
d'allocation réelle devait être exclue avant de publier une conclusion.

## Établi

1. **Le chemin d'erreur du prologue n'a jamais été pris, sur preuve
   directe et non sur relecture manuelle.** `heap+20` (le champ testé par
   `rlwinm. r11,r11,0,13,13`) vaut `0x00000002` dans les deux instantanés
   captures — le bit `0x40000` est à 0, donc `beq loc_821F9E78` est pris
   et le prologue saute PAR-DESSUS l'appel à `KeGetCurrentProcessType`.
2. **Confirmé indépendamment en rejouant les deux cas avec les deux
   imports stubés en plus** (`stub 0x823D03FC`/`0x823D03EC`, ajoutés aux
   deux specs) : résultat strictement identique
   (`steps=212`/`127`, `callee_entries=0`, `stubbed_calls=2` — toujours
   seulement les deux stubs de section critique, les deux nouveaux stubs
   n'ont jamais été atteints). Ceci ferme la question du reviewer : la
   trace n'a jamais marché sur un import non modélisé.
3. **Les instantanés post-exécution (`dump heap`) montrent une écriture
   substantielle et cohérente, pas un retour trivial** :
   - `call1` (taille=1) : 9 plages modifiées, dont le mot à
     `0x10000184` (`0x1009fa18` -> `0x10082ac8`, un champ de la table
     sentinelle) et le mot à `0x1009fa18` (`0x10000180` ->
     `0x10082ac8`, le propre champ de chaînage du nœud `0x1009fa10`),
     plus plusieurs champs du bloc `0x10082aa0..0x10082ad0` lui-même,
     dont deux mots à `0x10082ac8`/`0x10082acc` qui prennent
     exactement les valeurs `0x10000180` (adresse de la sentinelle) et
     `0x1009fa18` (adresse du champ de chaînage de l'ancien voisin).
   - `call2` (taille=256) : seulement 2 octets modifiés
     (`0x10000033`, `0x10082aa6`), une empreinte bien plus petite.
   Voir `artifacts/retail-us-native-r402-mechanism-bytes/heap_diffs.txt`
   pour le détail complet.

## Non établi

**La signification sémantique exacte des champs écrits.** Les valeurs à
`0x10082ac8`/`0x10082acc` (adresse de la sentinelle, adresse du champ de
chaînage de l'ancien voisin) RESSEMBLENT à une insertion en tête de liste
doublement chaînée — mais elles pourraient tout aussi bien être des tags
de bornage (boundary tags) physiques utilisés pour la coalescence au
`free()`, une convention distincte du chaînage par bucket de taille déjà
tracé dans le corps de la fonction (§ ci-dessous). Les deux lectures ne
sont pas départagées par les octets seuls ; il faudrait lire la suite du
corps de `sub_821F9E10` (au-delà de `loc_821F9EC4`/`loc_821F9F80`, jamais
lu en entier ce cycle) pour trancher, avec la même discipline "vérifié en
direct avant conclusion" que le reste de cette campagne.

Note technique en passant, elle aussi non résolue ce cycle : une lecture
manuelle du bucket de taille (`r29 = (taille+31)&~15 >> 4` : 2 pour
taille=1, 17 pour taille=256 — donc PAS une collision d'indice de bucket,
hypothèse explicitement écartée) a montré que le bucket 2 est VIDE
(auto-pointeur, `0x10000190`) dans l'instantané `call1`, alors que le
bucket 17 a une tête réelle (`0x10082aa8`, PAS `0x10082ab0` — un écart de
8 octets non expliqué) dans l'instantané `call2`. Ceci indique que le
chemin réellement pris par `call1` n'est probablement PAS le chemin de
retrait-simple depuis un bucket exact (lu par erreur comme le chemin
probable avant ce contrôle), mais un autre chemin du même corps de
fonction (branche `loc_821F9F80`, jamais lue ce cycle) — nommé pour r403,
pas deviné ici.

## Ce que ce cycle change par rapport à r401

Le verdict de r401 ("concordance, pas de mauvaise traduction de codegen
dans `sub_821F9E10`") **tient et est maintenant mieux fondé** : la trace
a bien exécuté une logique substantielle et réelle (écritures multiples,
cohérentes avec des adresses de structure déjà connues de la campagne),
pas un chemin d'erreur court-circuité par un import non modélisé. Le
message du commit r401 n'est donc pas rétracté. Ce qui est ajouté ici est
la preuve directe (pas seulement l'absence de contre-preuve) et l'aveu
honnête que le MÉCANISME exact (quel champ signifie quoi) reste ouvert.

## Décisions prises

- Rejouer avec des stubs défensifs supplémentaires plutôt que de publier
  la conclusion de r401 sur la seule lecture manuelle du prologue, suite
  à la relecture d'un reviewer avant publication — corrige la discipline
  de preuve plutôt que la contourne.
- Ne pas deviner la sémantique des champs écrits sans lire le corps de
  fonction correspondant en entier — nommé pour r403 plutôt que conclu
  ici, conformément à la discipline du projet (précédent r1111/r1113).

## Gate

`ctest` inchangé (aucune source de production éditée ce cycle).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF -> exit 0
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json -> exit 0
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json -> exit 0
```

## Named for r403

Read `sub_821F9E10` in full from `loc_821F9EC4` (the bucket-size check)
through both `loc_821F9F80` and the unlink path already partially read in
r401/r402, to determine: (a) which branch `call1` (bucket 2, empty)
actually took, given it still returns `0x10082ab0` with substantial
writes; (b) whether the fields at `0x10082ac8`/`0x10082acc` are freelist
chain pointers or physical boundary tags; (c) whether the observed
9-write/2-write asymmetry between the two calls is consistent with "call1
splits/inserts a remainder, call2 finds the resulting live block already
double-visible to bucket 17" — the shape that would finally explain the
double-issue without a codegen bug, or something else. Continue using the
now-working `ac6-us`-scoped microexec harness (r401) rather than
live-only observation where possible.

## Files

`recompilation/ace-combat-6-retail/artifacts/retail-us-native-r402-mechanism-bytes/heap_diffs.txt`.
Updated specs:
`analysis/microexec/calibration/r401-sub821f9e10-call1-size1.spec`,
`...call2-size256.spec` (added the two defensive stubs and `dump heap`).

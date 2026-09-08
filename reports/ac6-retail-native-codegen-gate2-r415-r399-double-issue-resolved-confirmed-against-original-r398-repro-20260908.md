# AC6 retail NTSC-U/J — r415 — le double-octroi original de r399 a disparu : rejoué le repro exact de r398, les deux requêtes reçoivent désormais des adresses distinctes, déterministe sur 2 lancements

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r414 (nommé pour r415) : rechercher si un double-octroi
équivalent à `seq≈874`/`877` (r401-r409) se produit encore ailleurs
dans la séquence désormais bien plus longue, ou si le correctif de
r414 ferme réellement le blocage r399 original.

## Établi

### Le repro exact de r398 (le symptôme original de r399) est rejoué sans aucune modification

`r398_probe.gdb` (`artifacts/retail-us-native-r398-alloc-sequence/`)
capture, dans `sub_8236E868`, la valeur de retour de quatre requêtes
d'allocation consécutives (`+150`=784o, `+225`=55944o, `+294`=1o,
`+343`=256o) au même point d'observation utilisé par r398. **r398
avait établi, déterministe sur 3 lancements indépendants** :

```
CALL off=294 req_size=1     returned_r3=0x10082ab0
CALL off=343 req_size=256   returned_r3=0x10082ab0   <-- IDENTIQUE
```

**Rejoué sans aucune modification sur le binaire corrigé par r414
(reconstruit ce cycle), déterministe sur 2 lancements indépendants :**

```
CALL off=150 req_size=784   returned_r3=0x10024200
CALL off=225 req_size=55944 returned_r3=0x100a4520
CALL off=294 req_size=1     returned_r3=0x100b1fc0
CALL off=343 req_size=256   returned_r3=0x100b1fe0   <-- DIFFÉRENT
```

**Les quatre adresses sont désormais toutes distinctes**, et
identiques d'un lancement à l'autre (adresses invitées, non affectées
par le PIE re-basé à chaque lancement — cohérent avec la mise en garde
déjà établie par r385). Le double-octroi original — la SEULE preuve
directe et déterministe qui a motivé la qualification "r399" comme
blocage — **ne se reproduit plus**.

## Ce que ceci établit pour la chaîne r399-r414

**La chaîne causale complète est maintenant vérifiée dans les deux
sens** : r413 a tracé le mécanisme exact (un correctif antérieur,
r366, écrasait sans le vouloir la valeur de retour de
`sub_821FA9E0`) ; r414 a appliqué et vérifié en direct que ce
mécanisme est corrigé (le pointeur correct se propage désormais de
bout en bout) ; **ce rapport confirme que le symptôme original qui a
ouvert cette investigation — le double-octroi capturé par r398 —
disparaît avec ce correctif, sur le repro exact utilisé pour l'établir
la première fois.** Ce n'est pas une inférence indirecte (comme le
saut `883→67239` de r414) : c'est une répétition BYTE-POUR-BYTE du
protocole qui a produit la preuve originale, avec un résultat opposé.

## Non établi

- **Le mécanisme précis par lequel le débordement (r410-r412) produisait
  spécifiquement CE double-octroi** (`sub_8236E868`'s requêtes taille-1
  et taille-256 recevant la même adresse) — la chaîne causale complète
  entre "le tableau croissant du C++ statique déborde tôt dans
  `__xstart`" et "deux requêtes bien plus tardives, dans une fonction
  sans rapport, reçoivent la même adresse" n'a jamais été tracée
  explicitement lien par lien (seulement la disparition du symptôme
  final est vérifiée ici). Une reconstruction complète nécessiterait de
  relire la trace complète du heap corrompu que r401-r409 ont
  partiellement documentée, à la lumière de ce nouveau correctif — non
  fait ce cycle, la disparition du symptôme final est jugée une preuve
  suffisante pour clore ce fil sans cette reconstruction.
- **Si un autre double-octroi, sans rapport avec celui-ci, subsiste
  ailleurs dans le run désormais bien plus long** (`67239` événements
  contre `883`) — non recherché systématiquement, seul le repro exact
  connu a été revérifié.
- **`presented_frames=0`** (visible avant et après ce correctif,
  r414) — toujours sans rapport établi avec cette chaîne, ni examiné
  ce cycle.
- Aucun correctif supplémentaire proposé ni appliqué ce cycle.

## Décisions prises

- Rejouer le harnais `r398_probe.gdb` STRICTEMENT SANS MODIFICATION
  (copié tel quel, seul le nom du fichier de sortie a été changé pour
  ne pas écraser une trace historique déjà absente) plutôt que
  d'écrire une nouvelle capture — la preuve la plus forte contre le
  symptôme original est de répéter EXACTEMENT le protocole qui l'a
  établi, pas une variante.
- Considérer ce cycle comme la clôture appropriée du fil r399-r415 sans
  reconstruire la chaîne causale complète entre le débordement précoce
  et ce symptôme tardif précis — la disparition déterministe du
  symptôme sur le repro exact est jugée une preuve suffisante en
  l'absence d'un blocage qualifié exigeant plus (précédent r1111/r1113 :
  ne pas deviner un mécanisme non nécessaire à la décision).

## Gate

Aucune source de production éditée ce cycle (le binaire testé est celui
reconstruit par r414).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées dans le commit de ce cycle)
`ctest` relancé en entier.

## Files

`recompilation/ace-combat-6-retail/artifacts/retail-us-native-r398-alloc-sequence/r398_probe.gdb`
(rejoué sans modification),
`recompilation/ace-combat-6-retail/artifacts/retail-us-native-r398-alloc-sequence/r415_recheck_target.log`.

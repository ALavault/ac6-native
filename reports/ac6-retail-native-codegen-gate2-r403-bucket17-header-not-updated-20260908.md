# AC6 retail NTSC-U/J — r403 — mécanisme exact localisé : le nœud `0x10082aa0` a été à moitié inséré dans le bucket 17 (en-tête écrit, chaînage retour du nœud jamais écrit) ; le déchaînement gardé refuse à raison de le retirer, donc le bucket reste bloqué sur un bloc déjà scindé — le vrai défaut est en amont de `sub_821F9E10`, pas dedans

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r402 : lecture complète de `sub_821F9E10` depuis
`loc_821F9EC4` (contrôle de bucket exact) à travers `loc_821F9F80`
(recherche par bitmap hiérarchique sur les buckets plus grands) et
`loc_821FA168`/`loc_821FA1D4` (découpe et réinsertion du reliquat)
jusqu'au retour commun `loc_821FA528`.

Une première version de ce rapport concluait que le déchaînement à
triple vérification (`r9==r7`, `r9==r8`, `r10==r11`) était lui-même le
défaut ("gated unlink" fautif, même famille que r356/r382). **Cette
lecture était fausse et a été corrigée avant publication** — voir
"Correction" ci-dessous.

## Établi

1. **Format de bucket confirmé** (repris de la version précédente,
   inchangé) : 128 buckets par classe de taille, en-tête à
   `r30+(idx+48)*8`. Bucket vide = auto-pointeur. Bucket 0
   (`r30+384=0x10000180`) est juste le bucket de la plus grande classe de
   taille — pas une structure de tags de bornage séparée ; la question
   laissée ouverte par r402 est résolue, ce sont des pointeurs de
   chaînage de freelist.
2. **Retour final vérifié par calcul** : `loc_821FA528` fait
   `r30 = r26+16`, valeur finale retournée. `call2` :
   `r26 = bucket17_header[4]-8 = 0x10082aa8-8 = 0x10082aa0` ;
   `0x10082aa0+16 = 0x10082ab0` — exact.
3. **Le déchaînement à triple vérification EST le comportement
   correct, pas le défaut.** `headless3.log` (le rejeu microexec de
   r402) enregistre deux avertissements pour `call1` :
   `Uninitialized memory read at 821fa06c: ram:00000000:4` et
   `...821fa070: ram:00000004:4`. En comptant les instructions depuis
   `loc_821FA03C` (12 instructions avant `lwz r9,0(r11)`,
   `0x821FA03C+12*4=0x821FA06C`, confirmé), ces deux lectures SONT
   `lwz r9,0(r11)` et `lwz r7,4(r10)` — c'est-à-dire que **`r11` et
   `r10` valent zéro : les champs `+8`/`+12` du nœud trouvé
   (`0x10082aa8`/`0x10082aac`) sont NULS**, confirmé directement en
   relisant ces deux adresses dans `heap2_call1_size1.bin` (les deux
   valent `0x00000000`). Avec `r9=r7=0` et `r8=0x10082aa8`, la deuxième
   comparaison (`r9==r8`) ÉCHOUE, saute vers `loc_821FA168`, et les deux
   `stw` (qui auraient réécrit l'en-tête du bucket 17) ne s'exécutent
   jamais. **Le déchaînement refuse, à raison, de retirer un nœud dont
   les liens retour ne correspondent pas à son bucket.** Même
   raisonnement pour `call2`, avertissements symétriques à
   `0x821F9F04`/`0x821F9F08` (chemin bucket-exact, même paire de champs
   nuls, même échec de comparaison, même saut sans écriture).
4. **Le vrai défaut est donc antérieur à ces deux appels** : le nœud
   `0x10082aa0` a été inséré dans le bucket 17 de façon INCOMPLÈTE —
   l'en-tête du bucket (`0x10000208`, `[0]=[4]=0x10082aa8`) a été écrit,
   mais les champs de chaînage retour du nœud lui-même
   (`0x10082aa8`/`0x10082aac`, censés pointer vers `0x10000208` pour une
   liste circulaire à un seul élément) n'ont jamais été écrits — restés
   à leur valeur d'origine (zéro). C'est cette moitié d'insertion
   manquante, PAS `sub_821F9E10`, qui rend le bucket 17 durablement
   bloqué : toute allocation qui y cherche un nœud le retrouve (l'en-tête
   dit qu'il est là) mais aucune ne peut jamais le retirer proprement (le
   déchaînement refuse toujours, correctement, sur des liens à zéro) —
   ce qui explique le double-octroi observé sans invoquer un bug dans
   cette fonction elle-même. **Confirme et affine le verdict de
   r401/r402** (pas de mauvaise traduction de codegen dans
   `sub_821F9E10`) plutôt que de le contredire.
5. **Confirmation secondaire** : les deux octets modifiés à
   `0x10000033` par r402 (`0x34->0x32` pour `call1`, `0x32->0x21` pour
   `call2`) sont `stw r10,48(r30)` (`loc_821F9F50`/`loc_821FA168`,
   décrément d'un compteur d'octets libres par `r29`, l'indice de
   bucket : `50-2=48=0x30`? — valeurs `0x34->0x32` = décrément de 2,
   `0x32->0x21` = décrément de 17, cohérent avec `r29=2` puis `r29=17`).
   Confirme que les deux appels ont bien atteint la logique de
   comptabilité post-déchaînement, pas un chemin d'erreur.

## Correction

La première version de ce rapport (non publiée) attribuait le défaut au
déchaînement à triple vérification lui-même, par analogie avec
r356/r382. **C'était une erreur** : le déchaînement fonctionne
exactement comme prévu ici — il refuse de retirer un nœud dont les
champs retour ne pointent pas vers son bucket, ce qui est le
comportement correct face à des données déjà corrompues en amont.
Proposer un correctif qui ajoute les `stw` manquants sur CE chemin (le
réflexe "miroir de la logique sœur déjà correcte" qui a fonctionné pour
r356/r358/r366/r376/r383) aurait écrit l'en-tête du bucket 17 à partir de
liens à zéro — un correctif qui aurait aggravé la corruption plutôt que
de la réparer. Corrigé avant publication, pas après.

## Non établi

**Qui a écrit l'en-tête du bucket 17 (`0x10000208`) sans écrire les
champs retour du nœud (`0x10082aa8`/`0x10082aac`).** Deux candidats déjà
visibles dans le corps de cette même fonction, ni l'un ni l'autre lu en
entier ce cycle : le chemin de réinsertion du reliquat
(`loc_821FA1D4`, lu seulement jusqu'à `bne cr6,loc_821FA2F4`) et le
chemin de bloc neuf via `NtAllocateVirtualMemory`
(`loc_821FA564`/`loc_821FA58C`). `sub_821FA6F8` (chemin `free`) reste un
candidat externe, mais r356 a déjà audité sa logique de scission comme
correcte.

## Décisions prises

- Ne pas publier la première lecture (déchaînement fautif) une fois
  qu'un contrôle direct (relecture des octets à `0x10082aa8`/`0x10082aac`)
  l'a réfutée — corriger avant publication plutôt que de laisser une
  conclusion non vérifiée entrer dans `NEXT.md`.
- Ne toujours pas appliquer de correctif tant que l'écrivain exact de
  l'en-tête à moitié inséré n'est pas tracé en direct — même discipline
  que r380/r389 (précédent r1111/r1113).

## Gate

`ctest` inchangé. Trois audits de contrat repassés au vert :
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF -> exit 0
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json -> exit 0
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json -> exit 0
```

## Named for r404

Armer un point d'observation matériel sur `0x10000208` (en-tête du
bucket 17) ET séparément sur `0x10082aa8`/`0x10082aac` (les champs
retour jamais écrits), depuis le tout début du processus (technique de
r371/r389 — explicitement nommée par r389 pour la sentinelle et jamais
exécutée). Capturer quelle fonction écrit l'en-tête SANS écrire les
champs retour du nœud — la même asymétrie que le trio ci-dessus laisse
ouverte. Une fois l'écrivain identifié, le lire en entier et déterminer
si le correctif est un `stw` manquant sur SON chemin (pas sur
`sub_821F9E10`), en direct avant tout correctif.

## Files

`recompilation/ace-combat-6-retail/artifacts/retail-us-native-r401-microexec-discriminator/headless3.log`
(déjà présent, warnings analysés ce cycle) ; aucune nouvelle capture live
ce cycle, uniquement lecture de source et vérification par script Python
contre les instantanés déjà capturés par r401/r402.

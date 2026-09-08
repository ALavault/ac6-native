# AC6 retail NTSC-U/J — r444 — localisé EN DIRECT le site d'échec exact à l'intérieur de `sub_821CC508` : un appel à `sub_821D4988` juste avant le retour `-1` confirmé — deux sites `-1` distincts existent dans cette fonction, celui réellement emprunté est identifié précisément

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r443 : décompiler `sub_821CC508` pour comprendre quelle
condition la fait retourner `-1`, et tracer `sub_821CC008` pour
identifier le fichier/chemin concerné.

## Établi — en direct

### `sub_821CC508` contient DEUX sites de retour `-1` distincts, pas un seul

Désassemblage en direct (processus arrêté) de la fonction bornée
correctement : deux occurrences de `movq $0xffffffffffffffff,(%rbx)`
(le stockage du retour `-1` dans `ctx.r3`), aux offsets `+6046` et
`+6171` relatifs à l'entrée de la fonction — cette fonction est une
grosse machine à états (motif `switch`/`goto LAB_...`/`code_r0x...`
déjà visible dans la décompilation de r443), avec plusieurs chemins
d'échec possibles.

### Le site réellement emprunté est identifié précisément

Points d'arrêt posés sur les DEUX sites `-1` plus le premier appel au
probable équivalent de `GetLastError` (`sub_821F50A0`) rencontré dans
le désassemblage. Résultat du lancement réel :
**Breakpoint 3 (`+6171`, le SECOND site) se déclenche** — le premier
site (`+6046`) et l'appel `GetLastError`-équivalent ne se déclenchent
PAS. Le processus enchaîne directement vers le crash déjà connu de
`sub_821D6C20` juste après.

Contexte immédiat de ce site (désassemblage) :
```asm
mov    %rcx,0x10(%rbx)
movq   $0xffffffff8275a414,(%rbx)   ; ctx.r3 = constante 0x8275a414
mov    %eax,(%r14,%rdx,1)
mov    %rbx,%rdi
mov    %r14,%rsi
call   sub_821D4988                  ; appelée SANS vérification de son retour
movq   $0xffffffffffffffff,(%rbx)   ; retour -1, INCONDITIONNEL après cet appel
jmp    ...
```
**`sub_821D4988` est appelée avec la constante `0x8275a414` comme
argument, puis le retour `-1` suit SANS AUCUNE vérification du
résultat de cet appel** — ce n'est pas un chemin conditionnel de plus,
c'est la fin d'un traitement d'erreur déjà décidé (motif « journaliser
puis retourner l'échec », comme `sub_821F5B18` observé par r442 pour
un autre échec).

## Non établi

- **Le contenu de la chaîne/donnée à l'adresse invité `0x8275a414`** —
  une tentative de lecture (`x/s`) à l'adresse hôte correspondante a
  retourné une chaîne vide au moment où le point d'arrêt d'entrée de
  `sub_821CC508` était atteint ; soit l'adresse calculée est
  incorrecte, soit la donnée n'est pas (encore) initialisée à ce
  point, soit ce n'est pas une chaîne du tout (peut-être un code/une
  table). Non résolu ce cycle.
- **La sémantique exacte de `sub_821D4988`** — non décompilée ce
  cycle.
- **`sub_821CC008`** (le producteur de l'opération que `sub_821CC508`
  sonde, déjà partiellement caractérisé par r441 comme énumérant des
  fichiers réels) — pas encore tracé en direct pour confirmer quel
  fichier/chemin précis est en cause, comme nommé par r443.

## Décisions prises

- Poser des points d'arrêt sur LES DEUX sites `-1` plutôt que de
  supposer que le premier trouvé dans le désassemblage serait le bon
  — une première tentative n'ciblant que le premier site avait donné
  un résultat vide (aucun des deux points d'arrêt initiaux ne s'était
  déclenché), signal clair qu'il fallait élargir la recherche plutôt
  que de conclure prématurément.
- Ne pas pousser davantage la lecture de la chaîne à `0x8275a414` ce
  cycle — l'échec de lecture pourrait avoir plusieurs causes distinctes
  et mérite sa propre vérification plutôt qu'une supposition.

## Gate

Aucune source de production éditée ce cycle (investigation en direct
uniquement).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus, inchangées — aucune source de
production affectée)
`ctest` (racine et natif) inchangé depuis r443 (aucune source de
production modifiée).

## Named for r445

Deux pistes indépendantes : (1) décompiler `sub_821D4988` pour
comprendre son rôle exact et pourquoi la lecture de la chaîne à
`0x8275a414` a échoué (adresse mal calculée, donnée non initialisée,
ou pas une chaîne) ; (2) tracer en direct `sub_821CC008` (déjà
partiellement caractérisé par r441) pour identifier le fichier/chemin
exact impliqué dans l'opération qui échoue — les deux pistes,
combinées, devraient permettre d'identifier le vrai défaut sous-jacent
(fichier manquant, chemin mal résolu, ou stub hôte natif incomplet).
Reste ouvert sinon : décision de committage de l'arriéré
`native_vulkan_backend.cpp` (r433/r434/r438).

## Files

Aucun artefact gitignoré nouveau.

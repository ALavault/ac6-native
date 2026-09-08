# AC6 retail NTSC-U/J — r443 — pinpointé EN DIRECT : parmi les 4 conditions de `sub_821D5F48`, c'est la deuxième (`sub_821CC508` retourne `0xFFFFFFFF`) qui échoue — les vérifications 3 et 4 ne sont jamais atteintes ; `sub_821CC508`/`sub_821CC008` partagent le même descripteur `0x829ddd80`, cohérent avec une opération asynchrone de fichier/contenu qui échoue au lieu de réussir

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r442 : identifier laquelle des 4 conditions convergeant
vers `loc_821D6138` échoue réellement, en posant des points d'arrêt
sur les fonctions appelées par chacune plutôt que sur les sites de
branchement eux-mêmes (dont la corrélation adresse-hôte s'était
révélée coûteuse à cause du réordonnancement du compilateur).

## Établi — en direct

### Points d'arrêt sur les 4 callees + le point d'entrée du crash déjà connu

`sub_821F4078` (vérification 1) ignoré après le 1ᵉʳ coup (appelé de
façon répétée, visiblement ailleurs aussi — ne bloque pas la
progression). Puis, dans l'ordre d'exécution réel observé :

```
Breakpoint 2 (sub_821CC508, vérification 2) — atteint UNE fois
Breakpoint 5 (sub_821D6C20, crash déjà connu) — atteint ensuite
SIGSEGV
```

**`sub_821D28C8` (vérification 3) et `sub_821D5600` (vérification 4)
ne sont JAMAIS atteints.** Puisque ces deux vérifications se trouvent,
dans le source, ENTRE la vérification 2 et le bloc d'écriture de
`*0x82935D98`, leur absence totale confirme que l'exécution quitte la
fonction juste après (ou à cause de) la vérification 2 — pas plus
loin.

### La valeur de retour réelle de `sub_821CC508`, lue à l'adresse hôte exacte

Désassemblage EN DIRECT (processus arrêté, pas statique) de la
fenêtre `[sub_821D5F48, sub_821D6C1C)` pour localiser précisément
l'instruction `mov (%rbx),%eax` qui lit `ctx.r3` juste après l'appel
(`call sub_821CC508` à l'offset `+2301`) :
```asm
call   sub_821CC508
mov    (%rbx),%eax      ; ctx.r3 = valeur de retour
test   %eax,%eax
je     <boucle si == 0>
test   %eax,%eax
js     <échec si < 0>   ; loc_821D6138 (le point de sortie précoce)
```
Point d'arrêt posé exactement sur cette instruction (adresse hôte
calculée par rapport à `$pc` à l'entrée de la fonction, confirmée en
direct, pas une adresse statique devinée) :

```
sub_821CC508 return eax=0xffffffff (checked at +2308)
```

**`0xFFFFFFFF` = `-1` en signé — le drapeau de signe est posé, le
branchement `js` vers l'échec se déclenche immédiatement**, sans
jamais reboucler (retour ≠ 0) ni continuer vers les vérifications
3/4. Ceci correspond exactement au comportement observé (vérifications
3/4 jamais atteintes).

## Ce que ceci établit

**Le site de défaillance précis est identifié : `sub_821CC508(r3 =
0x829ddd80)` retourne `-1` au lieu de `0` (en attente, ferait
reboucler) ou d'une valeur positive (prêt, ferait continuer).**

`sub_821CC508` partage le même argument constant (`0x829ddd80`,
visible dans le désassemblage : `movq $0xffffffff829ddd80,(%rbx)`
juste avant chaque appel) avec `sub_821CC008` — la fonction analysée
par r441 qui énumère des **fichiers réels par chemin** (traduction
`/`→`\`, ouverture `Function_821F4C10`, taille `Function_821F4E08`,
fermeture `Function_821F47D8`) et accumule une taille. Le motif de
`sub_821CC508` (boucler tant que `0`, échouer si `<0`, continuer si
`>0`) est le motif classique d'un **sondage de complétion d'opération
asynchrone** : `sub_821CC008` initie/prépare probablement une
opération de contenu/fichier référencée par ce même descripteur
`0x829ddd80`, et `sub_821CC508` en sonde le statut. **Dans cet
environnement offline, cette opération se termine en erreur au lieu
de réussir.**

`sub_821CC508` est aussi appelé par `sub_821D6C20` lui-même (même
signature, `Function_821CC508(0xffffffff829ddd80)`, dans une boucle
`do...while` identique) — cohérent avec un utilitaire de sondage
générique réutilisé par plusieurs sous-systèmes autour du même
descripteur d'opération.

## Non établi

- **Ce que `0x829ddd80` représente précisément** (structure de
  requête, quel fichier/contenu) et **pourquoi l'opération échoue**
  dans cet environnement offline (fichier manquant, chemin mal
  résolu, stub hôte incomplet pour l'appel système sous-jacent) — non
  tracé ce cycle.
- **Ce que `sub_821F4078` (vérification 1, ignorée) fait exactement**
  — appelé de façon répétée avec succès, hors du champ de ce cycle.

## Décisions prises

- Poser les points d'arrêt sur les callees plutôt que sur les sites
  de branchement — a évité entièrement le problème de corrélation
  adresse-hôte identifié comme coûteux par r442, en utilisant les
  symboles de fonction directement (technique déjà éprouvée cette
  session).
- Ne pas se fier à `finish`/valeur affichée par gdb pour lire `ctx.r3`
  — une première tentative (`finish` + `$eax`) a donné une valeur
  incohérente (`0x102`, positive) ; la lecture correcte nécessite de
  s'arrêter EXACTEMENT à l'instruction qui charge `ctx.r3` depuis la
  structure de contexte (le pointeur de retour C++ de ces fonctions
  `PPC_FUNC_IMPL` n'a pas de rapport avec la convention de retour PPC
  simulée) — cohérent avec le piège déjà documenté cette session sur
  les décalages `ctx.rN`.

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
`ctest` (racine et natif) inchangé depuis r442 (aucune source de
production modifiée).

## Named for r444

Décompiler `sub_821CC508` elle-même (bornes déjà connues,
`0x821CC508`-`0x821CD07C`, déjà désassemblée/créée dans le projet
Ghidra `ac6-us` par r441) pour comprendre précisément quelle
condition la fait retourner `-1`, et tracer en direct l'appel
`sub_821CC008` qui la précède (déjà partiellement décompilée par
r441 — motif d'énumération de fichiers) pour identifier le fichier/
chemin exact concerné et si un stub hôte natif (`NtCreateFile`/
`NtReadFile` déjà largement instrumentés ailleurs dans ce projet,
cf. r130-r131/r239-r240) est en cause. Reste ouvert sinon : décision
de committage de l'arriéré `native_vulkan_backend.cpp`
(r433/r434/r438).

## Files

Aucun artefact gitignoré nouveau.

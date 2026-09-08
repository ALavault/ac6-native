# AC6 retail NTSC-U/J — r413 — le commit manquant de r411 est expliqué : le correctif INVENTÉ de r366 (fuite de section critique) écrase la valeur de retour qu'il vient de recharger, sans jamais la restaurer

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r412 : le débordement du tableau croissant statique C++ est
désormais expliqué par une sentinelle `-1` mal comparée (r412), mais le
mécanisme du commit manquant de `begin` restait ouvert depuis r411 ("le
saut de commit (`loc_8237FAF0`) ne s'exécute jamais malgré un retour
non nul confirmé"). Ce cycle isole la cause exacte par capture directe
à chaque étage de la chaîne d'appel.

## Établi

### 1. Le retour se perd à l'intérieur de `sub_821FA9E0` — pas avant, pas après

`r424_return_chain.gdb`, trois points de capture en un seul run
(lecture systématique de `ctx.r3` à l'adresse mémoire correcte,
`ctx+0`, la même correction de méthode que r412) :

| étage | valeur lue |
|---|---|
| retour de `sub_821F9E10` (l'allocation elle-même, `seq=5`) | `0x100015a0` — correct |
| retour de `sub_821FA9E0` (le `realloc()`, `seq=6`) | **`0x0`** |
| retour de `sub_823857E0` (propage fidèlement) | `0x0` |
| `TEST-SITE` dans `sub_8237FA50` (`+204`, juste après le test) | `eax=0x0` |

Le pointeur correct entre vivant dans `sub_821FA9E0` et en ressort à
`0`. `sub_823857E0` et `sub_8237FA50` propagent fidèlement cette
valeur déjà perdue — aucun des deux n'est en cause (confirme et
précise r410).

### 2. Le mécanisme exact, localisé par désassemblage x86 statique

`sub_821FA9E0+2637` (`0x26b69d`) : `mov (%r14,%r12,1),%eax; bswap %eax;
... mov %rax,(%rbx)` — c'est le `lwz r3,356(r31)` du code PPC généré
(texte déjà cité par r410/r411) : il recharge CORRECTEMENT le nouveau
pointeur (`0x100015a0`) dans `ctx.r3` (`(%rbx)` = `ctx+0`).

`+2651` : `test $0x1,%r9b; je +2683` — teste le bit 0 du drapeau
"verrou tenu" (r23). Si le verrou N'a PAS été pris, saute directement
au retour — `ctx.r3` reste correct, AUCUNE perte.

**Si le verrou A été pris** (le cas de CE run, une croissance réelle
prend toujours le verrou) : `+2657` à `+2669` :
`add $0x580,%ebp; mov (%r14,%rbp,1),%eax; bswap %eax; mov %rax,(%rbx)`
— **ceci ÉCRASE `ctx.r3` avec le pointeur de la section critique**
(`PPC_LOAD_U32(r27.u32+1408)`, `1408 = 0x580` — correspond exactement
à l'offset utilisé par `RtlEnterCriticalSection`). Puis
`call RtlLeaveCriticalSection`. **`ctx.r3` n'est jamais restauré
après cet appel** — la fonction enchaîne directement sur
`mov %r15,0x8(%rbx)` (une valeur sans rapport, dans `ctx.r4`) puis
retourne.

### 3. Ce code appartient à un correctif INVENTÉ d'un cycle antérieur, pas au jeu retail

`recompilation/ace-combat-6-retail/tools/apply_sub_821fa9e0_leave_fix.py`
(non versionné, appliqué au source généré) documente : r364/r365/r366
ont trouvé une fuite de section critique dans `sub_821FA9E0` (aucun
appel `RtlLeaveCriticalSection` sur son unique chemin de retour, aucun
site frère correct trouvé nulle part dans la chaîne d'appel) et,
faute d'un site à imiter, ont **inventé** un correctif (décision
explicite de l'utilisateur, 2026-09-07) : insérer l'appel manquant
juste avant le retour, réutilisant `ctx.r3` pour porter le pointeur de
section critique — exactement comme le fait
`RtlEnterCriticalSection` plus haut dans la même fonction. Le
commentaire du correctif documente avec soin que le BIT du drapeau
survit à toutes les transformations intermédiaires, mais **ne
mentionne jamais que `ctx.r3` porte alors la valeur de retour de la
fonction elle-même**, rechargée deux lignes plus haut par le
`lwz r3,356(r31)` déjà présent. Le correctif écrase donc
silencieusement la valeur qu'il vient de charger.

## Ce que ceci établit pour r399, r410, r411, r412

**La chaîne causale complète est maintenant établie de bout en bout,
jusqu'à un correctif nommé et daté :**

1. Le tableau croissant statique C++ grandit correctement une première
   fois (`seq=5`/`6`) — alloc réussie, copie, libération de l'ancien
   tampon (r411).
2. `sub_821FA9E0` calcule et recharge correctement le nouveau pointeur
   (`0x100015a0`) dans sa valeur de retour — **CE rapport le confirme,
   contrairement à ce qu'on aurait pu craindre d'un défaut plus
   profond dans le `realloc()` lui-même.**
3. **Ce rapport identifie la cause exacte de sa perte** : le correctif
   `r366` (2026-09-07), en résolvant une fuite de section critique
   bien réelle et bien documentée, réutilise par inadvertance le même
   registre de retour comme registre de travail pour l'appel
   `RtlLeaveCriticalSection`, sans sauvegarde ni restauration.
4. `sub_8237FA50` reçoit donc `0` (échec) de `sub_823857E0`, ne commet
   jamais le nouveau pointeur dans `begin` (r411), et sa PROPRE
   logique de nouvelle tentative retombe alors sur le défaut de
   sentinelle `-1`/comparaison non signée documenté par r412 —
   expliquant pourquoi AUCUNE nouvelle croissance n'est plus jamais
   tentée pour le reste du run.
5. Ceci n'est **pas un bug du jeu retail** : c'est une régression
   introduite par un correctif appliqué à cette recompilation
   elle-même, sur du code qui n'existe pas sous cette forme dans le
   binaire Xbox 360 d'origine (le correctif est explicitement
   "INVENTÉ", pas restauré d'une logique retail correcte).

## Non établi

- **Le correctif exact à appliquer** — sauvegarder `ctx.r3` avant le
  bloc `if` et le restaurer après l'appel à
  `RtlLeaveCriticalSection` est la réparation évidente, mais n'a PAS
  été appliquée ni vérifiée ce cycle (aucune source de production
  éditée, conformément à la discipline habituelle de ces cycles de
  documentation). Nommé pour le cycle suivant.
- **Si ce défaut de fuite de section critique, une fois corrigé SANS
  ce nouveau défaut de valeur de retour, referme réellement toute la
  chaîne r399** — plausible au vu de r410/r411/r412, mais non vérifié
  par une exécution complète après correctif.
- **L'étendue des AUTRES appelants de `sub_821FA9E0` qui pourraient
  être affectés par la même perte de valeur de retour** — cette
  fonction a d'autres appelants potentiels que `sub_823857E0` (non
  recensés ce cycle).

## Décisions prises

- Ne pas appliquer le correctif de restauration de `ctx.r3` ce
  cycle — se limiter à établir et documenter la cause exacte,
  conformément au motif suivi par r399-r412 (cycles de documentation
  uniquement, aucune source de production éditée). Nommé pour le
  cycle suivant plutôt que décidé unilatéralement ici (précédent
  r1111/r1113 : ne pas deviner/agir sans un cycle dédié à la
  vérification du correctif).

## Gate

Aucune source de production éditée ce cycle.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées dans le commit de ce cycle)
`ctest` relancé en entier.

## Named for r414

Appliquer et vérifier en direct un correctif à
`tools/apply_sub_821fa9e0_leave_fix.py` (ou un correctif dédié) qui
sauvegarde `ctx.r3` avant le bloc `if ((r23.u32 & 1u) != 0) { ... }`
et le restaure juste après l'appel à `RtlLeaveCriticalSection`, avant
le `return`. Confirmer par capture live (même harnais que r424) que
`sub_821FA9E0` retourne désormais `0x100015a0` (ou l'équivalent) pour
cet appel, que `sub_8237FA50` commet effectivement `begin`, et
mesurer si le débordement documenté par r410/r411/r412 disparaît sur
un run complet.

## Files

`recompilation/ace-combat-6-retail/artifacts/retail-us-native-r404-bucket17-writer/r423_commit_site.gdb/.log`,
`r424_return_chain.gdb/.log`.

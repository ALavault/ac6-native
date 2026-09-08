# AC6 retail NTSC-U/J — r436 — le SIGSEGV de r435 est root-causé en direct : dispatch de vtable sur un pointeur d'objet global NUL à l'adresse invité `0x82935D98`, JAMAIS observé avant car cette fonction n'était jamais atteinte avant le déblocage r413

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r435, priorité élevée : root-causer le `SIGSEGV` reproductible
du thread d'entrée dans `sub_821D6C20`, avant de reprendre le travail
sur le contenu visuel réel.

## Établi — capturé en direct (gdb, sur le modèle méthodologique de r412/r420-r424/r426)

### Le site exact et l'instruction fautive

`gdb bt`/`info registers`/`x/i $pc-60` sur le crash reproduit :
```
0x0000555555724f58 in __imp__sub_821D6C20 ()  <__imp__sub_821D6C20+88>
   ... mov $0x82935d98,%ebp
       mov (%rsi,%rbp,1),%eax   ; lit *0x82935D98 (objet)
       bswap %eax
       mov (%rsi,%rax,1),%eax  ; lit *(objet+0) (vtable)
       bswap %eax
       add $0xe4,%eax           ; +228 (slot de vtable)
       mov (%rsi,%rax,1),%eax  ; lit *(vtable+228) (pointeur fonction)
       bswap %eax
       movabs $0xfffffffefbee0000,%rcx
=>     call   *(%rcx,%rax,1)    ; CRASH — table de dispatch indirect
```
Correspond exactement à la source générée
(`ppc_recomp.23.cpp:20296`, `codegen-20260831-mapfix-96838`) :
```c++
r30.s64 = -2104295424;                              // 0x82935D00
ctx.r3.u64 = PPC_LOAD_U32(r30.u32 + 23960);          // *0x82935D98
r11.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);              // *(r3+0)
r11.u64 = PPC_LOAD_U32(r11.u32 + 228);               // *(vtable+228)
ctr.u64 = r11.u64;
PPC_CALL_INDIRECT_FUNC(ctr.u32);                     // bctrl
```
**Codegen vérifié fidèle** : les six instructions x86 correspondent
une pour une à la source PPC générée, aucun désaccord — ceci n'est
PAS un bug de codegen.

### Le pointeur d'objet global est NUL — vérifié à chaque étape de la chaîne

Trois points d'arrêt gdb successifs, sur le même lancement
(`--probe-entry`, `AC6_NATIVE_ALLOW_ENTRY_PROBE=1`,
`AC6_NATIVE_PROBE_WINDOW_MS=25000`) :

```
object_ptr(r3) = 0x0        (*0x82935D98, avant tout accès)
vtable_ptr(r11=*r3) = 0x0   (*(r3+0), donc *(NULL+0))
ctr_candidate = 0x0         (*(vtable+228), donc *(NULL+228))
```

**Chaîne complète confirmée** : le pointeur d'objet global stocké à
l'adresse invité `0x82935D98` vaut `0x0` au moment de cet appel. Tout
le reste (la « vtable » lue, le « slot 228 » lu) n'est que la lecture
de zéros en aval d'un pointeur nul — pas des données corrompues, un
déréférencement nul classique. `PPC_CALL_INDIRECT_FUNC(0)` tente
ensuite de résoudre l'adresse de fonction invité `0x0` dans la table
de mappage host, échoue à trouver une entrée valide, et le calcul
résultant (`rcx + rax` avec `rax` dérivé de `0`) déréférence une
adresse host non mappée → `SIGSEGV`.

### Pourquoi ceci n'a jamais été observé avant ce cycle

`sub_821D6C20` a été extensivement étudiée aux cycles r345-r384
(`artifacts/retail-us-native-r346-d6c20-dispatch-survey/` etc.) —
r346 avait alors mesuré, avec la même rigueur de mesure directe, que
**cette fonction n'était JAMAIS atteinte, même sur 90 secondes**, le
thread d'entrée restant bloqué bien plus tôt (dans
`sub_821D5F48`/l'allocateur, avant la clôture de la chaîne r399 par
r413-r415). Ce n'est qu'après r413 (correctif de l'allocateur) et
r420-r424 (correctif IRQL) que l'exécution progresse assez loin pour
atteindre — et maintenant planter dans — ce site, jamais exercé
auparavant. **Ce n'est pas une régression introduite par un cycle
récent : c'est un défaut préexistant révélé pour la première fois
par le déblocage de la chaîne r399.**

## Non établi

- **Quel sous-système devrait peupler le pointeur global à
  `0x82935D98`** avant ce point du boot, et pourquoi il ne l'a pas
  fait — non tracé ce cycle (nécessiterait de chercher tous les sites
  d'écriture statique/dynamique à cette adresse, sur le modèle des
  techniques déjà utilisées pour d'autres pointeurs globaux dans ce
  dépôt).
- **Si un stub hôte manquant est la cause** (un service Xbox 360 non
  modélisé dont l'initialisation réelle peuplerait normalement cet
  objet) **ou si un correctif d'un cycle antérieur (r413/r422/r430)
  a débloqué l'exécution jusqu'à ce point sans que le code qui
  peuple normalement cet objet ait eu la chance de s'exécuter** (ordre
  d'exécution altéré) — les deux hypothèses restent ouvertes,
  aucune preuve ne les départage encore.
- **Si ce même schéma (pointeur global nul, jamais peuplé) affecte
  d'autres adresses similaires** rencontrées plus loin dans le boot —
  seul ce site précis a été confirmé.

## Décisions prises

- **Ne pas corriger ce cycle.** Root-causer la valeur nulle
  elle-même (pourquoi cet objet n'existe pas) est un préalable
  nécessaire avant tout correctif — une règle plausible sans contrôle
  (par exemple, contourner l'appel indirect sans savoir ce qu'il
  devait faire) serait refusée par la discipline de ce dépôt.
- **Documenter la chaîne complète avec des valeurs mesurées à chaque
  maillon**, pas seulement l'adresse de crash — la discipline de ce
  dépôt (r1133/r1134) exige de ne jamais s'arrêter à une affirmation
  non vérifiée quand la vérification est possible à faible coût, ce
  qui était le cas ici (3 points d'arrêt successifs sur le même
  lancement).

## Gate

Aucune source de production éditée ce cycle (investigation en direct
uniquement : `gdb`, lecture de `ppc_recomp.23.cpp`).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus, inchangées depuis r435 — aucune source
affectée)
`ctest` (racine et natif) inchangé depuis r435 (aucune source
modifiée).

## Named for r437

Tracer qui devrait peupler le pointeur global à l'adresse invité
`0x82935D98` : chercher les sites d'écriture (statiques dans le XEX,
ou dynamiques via un stub hôte qui construirait cet objet) pour
déterminer si (a) un stub hôte manquant/incomplet est en cause, ou
(b) un correctif antérieur (r413/r422/r430) a modifié l'ordre
d'exécution au point de sauter l'initialisation normale de cet objet.
Tant que cette cause n'est pas établie, ne pas tenter de corriger
`sub_821D6C20` lui-même — le défaut est en amont.

## Files

Aucun artefact gitignoré nouveau (journaux de capture sous
`/fastdata/tmp/...scratchpad/`, non conservés).

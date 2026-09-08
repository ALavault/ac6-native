# AC6 retail NTSC-U/J — r435 — adresses invité réelles capturées en direct : `fetch_const` réel dépasse largement les 64 Mio du SSBO à décalage direct (confirme r434) — ET un SIGSEGV reproductible du thread d'entrée découvert en cours de route, non lié à cette instrumentation, contredisant la clôture « aucun blocage connu » de r431/r432

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r434 : capturer en direct les adresses invité réelles qu'un
vrai `DrawPacket` transporte, pour trancher si le SSBO à décalage
direct de 64 Mio de `PinnedShaderRuntime` peut fonctionner tel quel.

## Correctif/instrumentation appliqué (diagnostic uniquement)

`native/src/native_guest_vd.cpp`, dans le bloc de comptage déjà
présent (r294) : ajout d'une trace par `DrawPacket` réel
(`index_address`, `vertex_address`, `vertex_count`, `index_count`) et
d'un déversement des dwords non nuls du bloc de registres de
constantes de fetch (`kRegShaderConstantFetch000`, 192 dwords),
gardés derrière le même `AC6_NATIVE_VD_TRACE` déjà utilisé partout
dans ce fichier — aucun changement de comportement hors trace.

## Établi — capturé en direct

Lancement `--probe-entry` avec `AC6_NATIVE_ALLOW_ENTRY_PROBE=1
AC6_NATIVE_VD_TRACE=1 AC6_NATIVE_PROBE_WINDOW_MS=25000`, plusieurs
tirages réels observés :

```
vd draw index_address=0x00000000 vertex_address=0x00000000 vertex_count=3 index_count=3
vd fetch_const[0]=0x1274027b
vd fetch_const[1]=0x10000056
...
vd draw index_address=0x00000000 vertex_address=0x00000000 vertex_count=3 index_count=3
vd fetch_const[0]=0x126c01df
vd fetch_const[1]=0x1000001a
```

**`index_address` est TOUJOURS `0x00000000`** sur tous les tirages
observés — cohérent avec `native_xenos.cpp` : ces tirages sont
auto-indexés (`DRAW_INDX` source ≠ 0, ou `DRAW_INDX_2`), pas des
tirages DMA depuis un tampon d'index invité. `vertex_address` est
également toujours `0x00000000` (jamais peuplé par le décodeur, déjà
établi par r434).

**`fetch_const[0]` porte l'adresse réelle.** Valeurs observées :
`0x1274027b`, `0x126c013b`, `0x126c01df`, `0x1274031f`, `0x1274027b`
(répété). Format de constante de fetch de sommet Xenos réel (bits bas
= type + drapeaux, adresse en octets dans les bits hauts) : ces
valeurs, interprétées comme adresse directe, se situent autour de
`0x126c0000`-`0x12740000` — **~310 Mio**, très largement au-delà des
**64 Mio (`0x04000000`)** du SSBO à décalage direct de
`PinnedShaderRuntime`.

## Ce que ceci établit

**Confirme précisément la préoccupation de r434, avec des adresses
réelles mesurées plutôt que déduites.** Router `execute_frame` vers
ce chemin réel sans modifier le contrat mémoire partagée échouerait
en `fail closed` sur CE tirage même (`byte_offset + needed >
shared_memory_dwords_ * 4u`, la garde déjà présente dans
`draw_pinned`) — pas une hypothèse, un fait mesuré. Le SSBO devrait
être significativement agrandi (au moins ~320 Mio pour couvrir les
adresses observées, sans garantie que ce soit la plage maximale
utilisée ailleurs dans le jeu) et/ou passer d'un adressage direct à
un remappage adresse-invité → décalage.

## Découverte séparée, non cherchée : un SIGSEGV du thread d'entrée, reproductible, contredit la clôture de r431/r432

Le premier lancement `--probe-entry` de ce cycle s'est terminé par un
`SIGSEGV` (`exit=139`, `core dumped`) au lieu du `presented_frames=5
state=2` attendu. **Vérifié comme non lié à cette instrumentation** :

1. Reproduit avec `AC6_NATIVE_VD_TRACE` désactivé (aucune sortie
   trace, seul le déversement de 192 registres reste actif) — même
   crash.
2. Reproduit avec le fichier `native_guest_vd.cpp` **remis exactement
   à l'état du commit `d7dc1682` (r432, AVANT toute modification de ce
   cycle)**, reconstruit, relancé — **même crash**, donc totalement
   indépendant du travail de ce cycle.
3. Reproduit 2 fois de plus avec le binaire de ce cycle (3/3 lancements
   se terminent en `SIGSEGV`).

Trace `gdb bt` du crash :
```
Thread 2 "ac6recomp" received signal SIGSEGV, Segmentation fault.
#0  0x0000555555724f58 in __imp__sub_821D6C20 ()
#1  0x0000555555726eed in __imp__sub_821D7DE0 ()
#2  0x000055555577c907 in __imp___xstart ()
#3  std::thread::_State_impl<...>::_M_run() ()
```
Le crash se produit dans du code PPC recompilé (`sub_821D6C20`,
appelé depuis `sub_821D7DE0`, appelé depuis `__xstart` — le thread
d'entrée lui-même), toujours après plusieurs cycles `vd drain
accepted`/`vd swap presented` réussis, jamais au tout début.

**Ceci contredit directement l'affirmation de clôture de r431/r432**
(« plus aucun blocage connu », « `presented_frames=5 state=2`
identique » observé de façon répétée). Soit ce crash est une
régression intermittente/dépendante du timing qui existait déjà
silencieusement (les lancements précédents ont simplement eu de la
chance sur la fenêtre de 25 s), soit quelque chose a changé dans
l'environnement d'exécution depuis. **Reproduit 3 fois sur 3 dans ce
cycle**, donc au minimum devenu beaucoup plus fréquent que ce que
r427-r432 ont observé (0 échec rapporté sur plusieurs lancements
chacun).

## Non établi

- **La cause du SIGSEGV dans `sub_821D6C20`** — adresse de crash
  nommée, pas encore la ligne PPC ni la cause (déréférencement nul,
  débordement de pile, corruption mémoire). Aucun désassemblage ni
  lecture du code généré effectués ce cycle.
- **Pourquoi ce crash n'a jamais été observé dans les nombreux
  lancements de r427-r432** — dépendance au timing/charge système
  suspectée, non confirmée.
- **La plage complète des adresses de fetch réelles** utilisées par
  le jeu au-delà de cette fenêtre de 25 s (le SIGSEGV coupe court à
  toute capture plus longue).

## Décisions prises

- **Ne pas tenter de corriger le SIGSEGV ce cycle** — découverte
  fortuite en cours de route sur une tâche différente (adresses de
  tirage), aucun désassemblage du site n'a encore été fait ; corriger
  à l'aveugle sans lire le code généré serait exactement le type de
  correctif refusé par la discipline de ce dépôt.
- **Garder l'instrumentation de trace** (`vd draw`, `vd
  fetch_const[N]`) dans le fichier committé — diagnostic à faible coût
  gardé derrière `AC6_NATIVE_VD_TRACE`, même précédent que la trace
  `r294` déjà en place dans ce même fichier.

## Gate

**Source de production éditée ce cycle** :
`recompilation/ace-combat-6-retail/native/src/native_guest_vd.cpp`
(déjà porteur d'un arriéré préexistant, divulgué depuis r430 ; ce
cycle ajoute uniquement les traces de diagnostic ci-dessus).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus)
`ctest` (racine et natif) relancés en entier.

## Named for r436 — priorité élevée

Le SIGSEGV reproductible prime sur la suite du travail de contenu
visuel : investiguer `sub_821D6C20`/`sub_821D7DE0` (désassemblage,
capture gdb de l'état registres au crash, comparaison au binaire
retail) pour déterminer s'il s'agit d'un bug de codegen ou d'un défaut
de robustesse d'un stub hôte — sur le modèle méthodologique déjà
établi (r412/r426 pour les pièges d'offset `ctx.rN`, r420-r424 pour le
défaut IRQL). Tant que ce crash n'est pas root-causé, toute affirmation
« la chaîne r399-r432 est close » doit être considérée comme
non fiable pour une fenêtre d'exécution longue.

## Files

Aucun artefact gitignoré nouveau (journaux de capture sous
`/fastdata/tmp/...scratchpad/`, non conservés).

# r589 — MissionTitle est un vrai HALT (le manager cesse d'être appelé), le nœud existe : « register the node » réfuté, trois fixes réfutés

## Qualification

- **Ghidra project** : `ghidra-projects/ace-combat-6`, `default.xex` (retail
  `ntsc-uj`). Décompilation : `exports/821b99b8.json`, `821b8430.json`,
  `821d2fc0.json`.
- **Oracle** : non. Aucun budget N3.
- **Preuve runtime** : hooks r588/r590 (`sub_821B9A00`, `sub_821B8478`,
  `sub_821D28C8`, `sub_821D3028`) + compteur brut `mgr_raw`. GPU saturé
  (`neural_amp`) → ~1 h/run ; ce cycle a coûté plusieurs runs.

## Résultat central : c'est un HALT déterministe, pas un blind-gate

`mgr_raw` compte **chaque** entrée de `sub_821B9A00`, indépendamment du gate de
trace ; `mode_updates` ne compte que les entrées gated. Pendant Opening, les deux
suivent (1681/1681). À MissionTitle, sur trois snapshots r563 successifs (émis
par un thread timer séparé, donc survivant à une boucle figée) :

```
ms=5400100  updates=1689  mode_updates=1689  mgr_raw=1689
ms=6000100  updates=1689  mode_updates=1689  mgr_raw=1689
ms=6600100  updates=1689  mode_updates=1689  mgr_raw=1689
```

`mgr_raw` est **plat** à 1689. Donc `sub_821B9A00` **cesse totalement d'être
appelé** après la 2ᵉ update de MissionTitle. Ce n'est pas un gate devenu aveugle
(sinon `mgr_raw` grimperait au-dessus de `mode_updates`) : la boucle de frame qui
dispatch le mode-manager **s'arrête**. Le thread d'entrée reste vivant
(`hrtimer_nanosleep` = pacing, r584), mais le chemin qui appelle `sub_821B9A00`
ne s'exécute plus. Et « exactement 2 updates puis arrêt » se reproduit à ≥3 runs
→ **logique guest déterministe**, pas un hang aléatoire.

## « Register the missing node » est réfuté

Hook `r590 lookup` (sur `sub_821D3028` = `Function_821D2FC0`, jamais hooké
avant) à MissionTitle :
```
r590 lookup root=829e6138 key=b362294c ret=18a30200   (x2, NON-NUL)
```
Le nœud sous la clé `0xb362294c` (= `mgr+0x54`) **existe** — le lookup renvoie
`0x18a30200`, un pointeur valide. Il n'est donc **pas manquant**. La conclusion
de r588 (« nœud que la HLE n'enregistre jamais ») est corrigée.

## Le désarme est un flip de disponibilité, pas un lookup nul

`Function_821B99B8:243-246` (vérifié) :
```c
if (*(mgr+0x24) == 2) {
  piVar5 = Function_821D2FC0(0x829e6218, *(mgr+0x54));   // 0xb362294c
  if (piVar5 == 0 || piVar5->vtable[0x20]() == '\0') *(mgr+0x24) = 0;  // désarme
  ...
```
Le nœud existant (§ ci-dessus), la branche `piVar5==0` est fausse. Le désarme
vient donc de **`node.vtable[0x20]() == 0`** : `f24=2` à upd 1688 (le poll tourne
→ readiness vraie), `f24=0` à upd 1689 (readiness fausse). Le poll unique
(`Function_821B8430`, `loadpoll upd=1688 dat=0 ret=0`) a **basculé la
disponibilité du nœud** en le traitant une fois. Le désarme est en aval de ce
flip, pas une cause indépendante.

## Trois fixes tentés, tous réfutés (règles sans contrôle → retirées)

1. **`DAT_8293ba10 = manager`** (entrée) — jamais appliqué (timing : le mode
   bascule *dans* l'update, mon check à l'entrée le rate).
2. **`DAT_8293ba10 = manager` + ré-arme `f24`** (post-update) — appliqué
   (`dat_was=0→188d0000`, ré-arme), mais **atterrit après** le poll de 1688 et
   n'empêche pas le halt (toujours 2 updates puis arrêt). Donc : le désarme `f24`
   n'est **pas** la cause du halt, et le contexte nul n'est pas le (seul) blocage.
3. Les deux ci-dessus **retirés** du code (write-to-guest sans effet démontré,
   interdit par le mode prototype). Les hooks de diagnostic sont conservés.

## Où est vraiment le blocage (relocalisé, non encore lu)

Le halt est **au-dessus** du mode-manager : l'**appelant** de `sub_821B9A00` (la
mise à jour de frame du jeu) cesse de le dispatcher après la 2ᵉ update de
MissionTitle. C'est une décision déterministe de la logique guest (un flag/état
posé par la 2ᵉ update gate le dispatch). Le nœud, le poll, `f24`, `DAT_8293ba10`
sont tous en aval et ne causent pas le halt.

**Prochaine mesure** : hooker l'appelant de `sub_821B9A00` (le dispatch de
frame) et logger, autour de MissionTitle+2, la condition qui gate l'appel — et
ce que la 2ᵉ update écrit qui la fait basculer. C'est GPU-borné comme les autres
(atteindre MissionTitle ~1 h).

## Décisions prises

1. **Réfuter « register the node »** sur preuve runtime (nœud présent), plutôt
   que de l'implémenter à l'aveugle.
2. **Retirer les trois fixes** : aucun n'avance le mode ; garder du code
   write-to-guest sans effet est une règle sans contrôle.
3. **Ne pas conclure au-delà de la preuve** : le halt est établi (mgr_raw plat) ;
   la condition exacte de l'appelant qui gate le dispatch reste non lue et est
   nommée comme prochaine mesure.

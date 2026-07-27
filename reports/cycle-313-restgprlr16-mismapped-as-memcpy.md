# AC6 cycle 313 — `__restgprlr_16` était déclaré comme `memcpy`

Une seule ligne de configuration bloquait le démarrage depuis le début.

**Résultat : le runtime ne s'interrompt plus, tourne en continu et présente des
images.** Frontière de démarrage : de 1,43 s à **plus de 110 s** sans arrêt.

## 1. Le défaut

La configuration contient une table `[rexcrt]` qui redirige des fonctions C du
jeu vers des implémentations natives :

```toml
[rexcrt]
memcpy = 0x82382F18      # <- faux
```

Or `0x82382F18` n'est pas `memcpy`. Les assistants de sauvegarde et de
restauration de registres sont une séquence contiguë de 4 octets de pas :
`0x82382F14` est `__restgprlr_15`, donc `0x82382F18` est **`__restgprlr_16`**.

Le corps réel à cette adresse le confirme sans ambiguïté :

```c
PPC_FUNC_IMPL(__imp__rex_sub_82382F18) {
    ctx.r16.u64 = PPC_LOAD_U64(ctx.r1.u32 + -136);   // ld r16,-136(r1)
    ctx.r17.u64 = PPC_LOAD_U64(ctx.r1.u32 + -128);   // ld r17,-128(r1)
    ...                                              // jusqu'à r31
```

C'est une restauration de registres non volatils, pas une copie mémoire.

## 2. Preuve par le corpus

Le décompte des assistants émis montre un trou exact :

```
275 __restgprlr_14      152 __restgprlr_20     1504 __restgprlr_27
139 __restgprlr_15      186 __restgprlr_21     2096 __restgprlr_28
  0 __restgprlr_16  <-  502 __restgprlr_22     2713 __restgprlr_29
 87 __restgprlr_17      433 __restgprlr_23
```

`__restgprlr_16` n'apparaît **jamais**, alors que ses voisins immédiats sont
émis des centaines de fois. À la place, **68 sites** appellent `rexcrt_memcpy`.

## 3. Conséquence

Toute fonction dont l'épilogue est `b __restgprlr_16` voyait sa restauration de
registres remplacée par un appel à `memcpy`. Ses registres non volatils
`r14`–`r31` n'étaient **jamais restaurés**, et l'appelant reprenait la main avec
un contexte corrompu.

C'est exactement le défaut poursuivi depuis le cycle 310 :

```
sub_821CD700 : 375 lignes, save=1, restore=0, écrit r28 (x4) et r30 (x3)
    // addi r1,r1,224
    // b 0x82382f18
    rexcrt_memcpy(ctx, base);     <- devait être __restgprlr_16
    return;
```

`sub_821CD700` est la cible réelle du `bctrl` de `sub_821D4ED0`. Elle écrasait
`r28` et `r30` de son appelant, d'où la section critique aberrante
`0x826A19B0`, d'où `dispatch type 130`, d'où l'abort.

Les 42 fonctions « sauvegarde sans restauration » relevées au cycle 311 sont la
signature de ce défaut, visible depuis deux cycles sans être reconnue.

## 4. Correctif

Une ligne :

```toml
# memcpy = 0x82382F18   <- WRONG: this address is __restgprlr_16
```

L'adresse est alors traitée comme la fonction qu'elle est, et l'épilogue appelle
`rex_sub_82382F18`, dont le corps restaure `r16`–`r31`.

## 5. Mesures

| | avant | après |
| --- | --- | --- |
| `REX_FATAL` | 0 | **0** |
| compilation / édition de liens | 0 erreur | **0 erreur** |
| sites `rexcrt_memcpy` | 68 | **0** |
| sortie du smoke | **134 (abort)** | **124 (survit)** |
| durée du journal invité | 1,43 s | **111 s**, sans fin |
| images par seconde | 0,00 | **0,86** |
| dessins hôte | 0 | **2** |
| `backend issue / success` | 26 / 26 | **28 / 28** |
| audio clients / file / pic | 0 / 0 / 0 | **1 / 2 / 3** |

Le runtime **présente des images** et fait tourner son audio. Il ne s'arrête
plus.

## 6. Ce qui n'est pas acquis

- 0,86 image par seconde et 2 dessins hôte : le jeu est vivant, pas jouable.
  L'écran ne montre que le panneau de diagnostic ; aucun contenu de jeu n'est
  rendu.
- Le correctif désactive une redirection native de `memcpy` ; le jeu utilise
  désormais sa propre implémentation. C'est correct, mais plus lent que la
  version native prévue. La vraie adresse de `memcpy` reste à identifier.
- Les autres entrées `[rexcrt]` n'ont **pas** été vérifiées. La même erreur peut
  s'y trouver, et le contrôle est mécanique : vérifier qu'aucune ne tombe dans
  la plage des assistants `__savegprlr_*` / `__restgprlr_*`.

`recompiler-generated` n'est pas `verified`.

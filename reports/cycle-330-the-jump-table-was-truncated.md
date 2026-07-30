# Cycle 330 — la table de saut était tronquée, et l'aiguillage portait sur la mauvaise valeur

## Cible

- Produit : AC6 Xbox 360 PAL, Xenon PPC big-endian, Xenos
- Module : `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`

## 0. Objet

P0.2 quater. Le cycle 329 a montré que l'invité ne boucle pas : il refaute sur un
`ud2`, à 100,00 % des échantillons, dans un aiguillage de table de saut mal
traduit. Ce cycle établit **pourquoi** et fournit le correctif.

## 1. La table réelle, lue en mémoire invitée

`scripts/ac6_read_guest_memory.py`, mapping qualifié contre l'instruction d'ancrage
`ld r11,16(r31)` en `0x82346140` avant toute lecture :

```
0x8267A1D0: 8237C828 8237C828 8237C640 8237C658 8237C670 8237C670
0x8267A1E8: 00000000 00000000 00000000 00000000 00000000 00000000
0x8267A200: 8237BF08 8237C688 8237C878 8237C688 8237BF38 8237C6A0
```

**Au moins six cibles.** Le générateur en a émis **deux** — les deux premières,
toutes deux `0x8237C828`, c'est-à-dire la tête de la fonction elle-même. Les
trois seules entrées qui *sortent* de l'aiguillage — `0x8237C640`, `0x8237C658`,
`0x8237C670` — ont été **perdues**. C'est pour cela que l'invité ne pouvait pas
progresser à travers ce dispatcher : les seules sorties n'existaient pas dans le
code généré.

## 2. Cause racine unique, deux symptômes

`function_scanner.cpp` identifie un `indexRegister`, puis `build_bctr` émet
`switch (r<indexRegister>.u32)` avec des cas **ordinaux** `case 0:, case 1:, …`.

Le code invité :

```
0x8237C840  lwz    r10,0(r4)            ; r10 = index brut
0x8237C844  rlwinm r10,r10,2,0,29       ; r10 = index*4   <- l'index est écrasé
0x8237C848  lwzx   r11,r10,r11          ; r11 = table[index], une ADRESSE
0x8237C84C  mtctr  r11
0x8237C850  bctr
```

Le `lwzx` **réutilise le registre de base de la table (`r11`) comme destination**
du chargement. Le scanner a donc désigné `r11` — qui contient l'adresse cible —
comme registre d'index. De cette unique erreur découlent les deux symptômes :

1. **L'aiguillage porte sur l'adresse.** `cmpl $0x2` contre une valeur
   `>= 0x82000000` n'est jamais vraie : les deux cas sont du code mort et seul
   `default: __builtin_trap()` est atteignable.
2. **La table est tronquée.** `scanForBounds` cherche une borne sur le registre
   d'index ; appliquée au mauvais registre, elle a rendu 2 au lieu d'au moins 6.

À noter : **aucun registre ne contient l'index simple au moment du `bctr`**,
puisque le `rlwinm` l'a écrasé par `index*4`. Un aiguillage par index est donc
**inexprimable** ici, quel que soit le registre choisi. Ce n'est pas un réglage
à corriger, c'est la forme de génération qui ne convient pas à ce motif.

## 3. Correctif 1 — aiguiller sur l'adresse calculée

Au `bctr`, `ctr` contient **déjà** l'adresse cible finale, pour tous les types de
table. Aiguiller dessus est donc correct par construction et ne demande aucune
supposition sur le registre d'index :

```c
switch (ctx.ctr.u32) {
case 0x8237C640: goto loc_8237C640;
case 0x8237C658: goto loc_8237C658;
…
default:
  REX_FATAL("Unlisted jump-table target {:#010x} at bctr 0x8237C850", ctx.ctr.u32);
}
```

Les entrées dupliquées, que les tables réelles contiennent, sont fusionnées : deux
étiquettes `case` identiques ne compilent pas. Les entrées nulles sont des trous
sans adresse à aiguiller, donc sans cas.

Le `default` devient un `REX_FATAL` nommant la branche et la valeur, au lieu d'un
`ud2` nu. C'est la leçon des cycles 312 et 329 : un défaut silencieux doit coûter
une ligne de journal.

Patch : `patches/rexglue-bctr-dispatch-on-address-20260730.patch`.

## 4. Correctif 2 — restituer les cibles perdues

L'aiguillage par adresse **ne répare pas la troncature** : avec une table de deux
entrées, l'invité atteindrait désormais un `REX_FATAL` nommé au lieu de tourner en
silence — un progrès de diagnostic, pas un dispatcher fonctionnel.

La configuration accepte `[[switch_tables]]`. L'entrée est fournie avec les six
cibles **mesurées**, non déduites :
`patches/ac6-switch-table-8237C850-20260730.toml`.

Les deux ensemble donnent un aiguillage correct pour ce site.

## 5. Ce qui est vérifié, et ce qui ne l'est pas

```text
table de saut lue en mémoire invitée, ancrage qualifié   6 cibles, 3 perdues
cause racine lue dans function_scanner + build_bctr      cohérente avec l'assembleur mesuré
compilation du correctif de génération                   rexglue lié, rc=0
git apply --check sur l'arbre de référence               vert, 3 correctifs
```

**Non vérifié de bout en bout.** Régénérer le corpus depuis cet arbre de travail
produit un résultat différent et cassé (`std::bad_alloc`, nombreuses cibles non
résolues) : l'arbre de travail `ac6-gapfill` porte des sources de génération
datant du cycle 313, divergentes de l'arbre de référence. Ce n'est pas un effet
du correctif — c'est la divergence des arbres, constatée au cycle 328 §0.

La vérification de bout en bout — régénération du corpus, reconstruction du
runtime de 165 Mo, nouvelle mesure des compteurs — reste donc à faire, **dans
l'arbre de référence**, et c'est l'étape suivante. Tant qu'elle n'a pas eu lieu,
ce cycle livre une cause racine mesurée et un correctif qui compile, pas un gain
d'exécution.

Ni preuve de jouabilité, ni preuve de parité retail.
`recompiler-generated` n'est pas `verified`.

## 6. Porte P0

**Non franchie**, inchangée : `eop` 34, `host_swap_presents` 12. Aucun compteur
ne bouge dans ce cycle.

## 7. Front suivant

1. Résoudre la divergence des arbres : aligner les sources de génération de
   `ac6-gapfill` sur l'arbre de référence, ou travailler directement dans
   l'arbre de référence.
2. Appliquer les trois correctifs, régénérer, reconstruire, mesurer.
3. Vérifier les **8 autres aiguillages dégénérés** énumérés au cycle 329 : chacun
   est une boucle infinie latente, et l'aiguillage par adresse les rend tous
   diagnosticables même si leur table est également tronquée.
4. Contrôler que les 742 aiguillages corrects le restent : l'aiguillage par
   adresse change leur forme émise, et une table incomplète qui « marchait » par
   hasard deviendra un `REX_FATAL` nommé — ce qui est le comportement voulu, mais
   doit être compté.

## 8. Règle ajoutée

**Quand un motif invité rend une forme de génération inexprimable, changer de
forme plutôt que d'améliorer la détection.** Ici, aucun registre ne porte l'index
au moment du branchement ; perfectionner l'identification du registre d'index
n'aurait jamais pu produire un aiguillage correct. La valeur sur laquelle le
matériel branche réellement — `ctr` — était disponible depuis le début et rend la
détection superflue.

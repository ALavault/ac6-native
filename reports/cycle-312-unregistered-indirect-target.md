# AC6 cycle 312 — cause racine : une cible d'appel indirect non enregistrée

Le cycle 311 avait localisé la corruption de `r28`/`r30` sans l'attribuer, et
avait écarté l'appel indirect. **Cette conclusion était fausse**, pour une
erreur de quatre octets. Ce cycle corrige, attribue la corruption à un mécanisme
précis, le corrige, et installe un détecteur permanent.

## 1. Correction du cycle 311 : la sonde était mal placée

Le cycle 311 affirmait que l'appel indirect n'était pas coupable, la sonde
posée « avant le `bctrl` » montrant les registres déjà corrompus.

Le code généré dit autre chose :

```c
	// bctrl
	ctx.lr = 0x821D4F30;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32);
	// mr r3,r28
	ac6CriticalSectionCalleeProbe(ctx.ctr, ctx.r28, ctx.r30);
```

`ctx.lr = 0x821D4F30` signifie que le `bctrl` est en `0x821D4F2C` et que
`0x821D4F30` est l'instruction **suivante**. La sonde s'exécutait donc **après**
l'appel, pas avant. Elle ne disculpait pas l'appel indirect : elle le désignait.

Avec les points de contrôle du cycle 311, la fenêtre se referme exactement :

```
loop head 0x821D4EF8  tid=d4d8: r30=0x829e6540 r28=0x829e6550  invariant tenu
(bctrl 0x821D4F2C, cible 0x821CCBE0)
post-bctrl 0x821D4F30 tid=d4d8: r30=0x00000000 r28=0x826a19b0  invariant ROMPU
```

Entre les deux, **un seul appel**. L'attribution est sans ambiguïté.

## 2. Ce qu'est `0x821CCBE0`

Ni une fonction émise, ni une entrée de configuration. C'est un **label** à
l'intérieur de `sub_821CC4D0` — la fonction même qui apparaissait dans la pile
du cycle 308.

Or le code qui le précède immédiatement est un épilogue complet :

```
        stw   r11,324(r31)
        addi  r1,r1,512          ; libération du cadre de pile
        b     0x82382F14         ; appel terminal vers __restgprlr_15
        return
loc_821CCBE0:                     ; <- cible du slot de table virtuelle
        li    r4,0
```

`0x821CCBE0` suit un **retour de fonction**. C'est donc une entrée de fonction
au sens strict, exactement comme `0x8238F434` au cycle 307, et l'analyse de
frontières l'a absorbée dans sa voisine.

## 3. Le mécanisme de corruption

```c
#define PPC_LOOKUP_FUNC(x, y) (*(PPCFunc**)(x + PPC_IMAGE_BASE + PPC_IMAGE_SIZE + \
                                            (uint64_t(uint32_t(y) - PPC_CODE_BASE) * 2)))
#define PPC_CALL_INDIRECT_FUNC(x) PPC_LOOKUP_FUNC(base, x)(ctx, base);
```

La table est indexée par adresse invitée. Une adresse jamais enregistrée y a un
emplacement **nul**, et l'appel déréférence ce pointeur nul.

Ce qui rend le défaut silencieux : ReXGlue installe un gestionnaire `SIGSEGV`
pour réaliser la MMIO invitée. La faute est donc **absorbée** par ce
gestionnaire, et l'exécution reprend avec un contexte corrompu au lieu de
s'arrêter. C'est pourquoi la corruption apparaissait 54 Mo plus loin, sous la
forme d'une section critique aberrante, sans jamais planter au point de faute.

## 4. Correctif appliqué

La configuration accepte des **fragments** (`parent = ...`), qui enregistrent une
adresse dans la table de fonctions sans la traiter comme une fonction autonome.

Déclarer `0x821CCBE0` seul enregistre bien la cible, mais scinde une queue
partagée et produit trois nouveaux pièges, tous atteints à l'exécution. En
déclarant **aussi** les trois cibles de branchement de cette queue, les
branchements deviennent des appels terminaux — qui préservent le contexte,
ReXGlue passant `ctx` explicitement :

```toml
0x821CC7A0 = { name = "rex_sub_821CC7A0", parent = 0x821CC4D0 }
0x821CC7AC = { name = "rex_sub_821CC7AC", parent = 0x821CC4D0 }
0x821CC800 = { name = "rex_sub_821CC800", parent = 0x821CC4D0 }
0x821CCBE0 = { name = "rex_sub_821CCBE0", parent = 0x821CC4D0 }
```

Résultat mesuré :

| | avant | après |
| --- | ---: | ---: |
| `REX_FATAL` dans le corpus | 0 | **0** |
| unités de traduction | 48 | 48 |
| compilation / édition de liens | 0 erreur | **0 erreur** |
| lignes de journal invité au démarrage | 569 | **594** |
| cibles indirectes non enregistrées | 1 | **0** |

## 5. Détecteur permanent

`PPC_CALL_INDIRECT_FUNC` est modifié pour vérifier l'emplacement avant de
l'appeler et **nommer** l'adresse fautive plutôt que de sauter dans le vide :

```
UNREGISTERED indirect target 0x821ccbe0 -- missing [functions] entry
```

Un défaut jusqu'ici silencieux et diagnostiquable en trois cycles devient une
ligne de journal. Sur le corpus corrigé, le compte est **zéro**.

C'est le principal acquis de ce cycle : la classe entière de défauts est
désormais détectable en une exécution.

## 6. Ce qui reste

Le démarrage s'arrête toujours sur `dispatch type 130 at 0x826a19b0`, à
594 lignes contre 569. La corruption de section critique **persiste** alors que
plus aucune cible indirecte n'est non enregistrée — sa source est donc
différente de celle corrigée ici, et reste à établir.

Étapes suivantes :

1. Rejouer les sondes d'invariant du cycle 311 sur le corpus corrigé : `r28` et
   `r30` sont-ils encore rompus, et dans quelle fenêtre ?
2. Étendre le détecteur aux emplacements **non nuls mais faux** — une table
   virtuelle mal résolue appelle une fonction réelle, mais la mauvaise, ce que
   le contrôle de nullité ne voit pas.
3. Examiner les 42 fonctions qui sauvegardent sans restaurer (cycle 311),
   `sub_821D4ED0` en faisant partie.

Aucun rendu d'image à ce stade ; l'objectif « première mission jouable » reste
derrière le démarrage.

`recompiler-generated` n'est pas `verified`.

## 7. Addendum, même cycle : `0x821CCBE0` n'est pas une entrée de fonction

Le §2 concluait que `0x821CCBE0` était « une entrée de fonction au sens strict »
parce qu'il suit un épilogue complet. Une fois la fonction réellement émise,
son corps dit le contraire :

| Mesure sur `rex_sub_821CCBE0` | Valeur |
| --- | ---: |
| lignes | 129 |
| `__savegprlr_*` | **0** |
| `__restgprlr_*` | **1** |
| écritures de `r30` | 1 |
| `return` | 4 |

Trois anomalies concordantes :

- **aucun prologue** — la première instruction est `li r4,0` ;
- **`r31` est utilisé sans être défini** (`addi r3,r31,344`), donc hérité de
  l'appelant ;
- **restauration sans sauvegarde** — elle appelle `__restgprlr_*` sans avoir
  jamais sauvegardé, ce qui écrase `r28`/`r30` de l'appelant avec le contenu de
  la pile.

Une méthode virtuelle établirait son propre cadre. Celle-ci n'en établit aucun
et dépend de l'état de registres de son appelant : c'est un **bloc de
continuation** du parent, atteint par branchement, non un point d'entrée.

Suivre un épilogue est donc une condition **nécessaire mais pas suffisante**
pour conclure à une entrée de fonction. Le critère utilisé au §2 — et déjà au
cycle 307 pour `0x8238F434` — est trop faible pris isolément ; il faut le
compléter par la présence d'un prologue.

**Conséquence sur le diagnostic.** Si `0x821CCBE0` n'est pas une fonction, le
`bctrl` qui l'appelle n'aurait jamais dû produire cette cible : `ctr` vient de
`vtable[1]` de l'objet `r29`, lui-même issu du parcours de liste en fin de
boucle. La cause est donc **en amont** — un objet ou un pointeur de table
virtuelle erroné — et non dans la fonction appelée.

Le correctif du §4 reste valable et mesuré (démarrage 569 -> 594 lignes, zéro
cible indirecte non enregistrée) : enregistrer l'adresse évite le saut par
pointeur nul. Mais il traite un symptôme. La question suivante est l'origine de
`r29`.

Étape suivante révisée : instrumenter le parcours de liste en fin de boucle
(`r29 = *(r11 + r31)`) et vérifier la validité de chaque nœud et de son pointeur
de table virtuelle avant le `bctrl`.

# START supprime l'avance du titre au lieu de la provoquer

Date : 2026-08-18
Sondes : deux runs de 12 000 ticks, même build, même store, la seule
différence étant l'injection de START au tick 3000.

## Correction préalable de mes propres mesures

Le commit `20fb6c48` a publié : « l'état du titre atteint 1 et ne bouge plus,
y compris à travers l'appui ». **C'est faux, et c'était un artefact de la
fenêtre** : elle faisait 3 600 ticks alors que le cycle d'attract en fait
4 003. Quatrième erreur de fenêtre trop courte de cette campagne, après
`KeSetEvent`, le sémaphore `0xE0000130` et la trace de thunk.

## Ce que fait le titre quand on ne touche à rien

```text
tick 222   démarrage 0x2E7F0080  state 0
tick 266                         state 1
tick 2426                        state 2
tick 2429  titre     0x2E3D0100  state 0
tick 2452                        state 1
tick 4252                        state 2      <- avance bien
tick 4255  démarrage 0x2E3E0080  state 0
...
tick 10458 titre                 state 1
```

Six entrées de mode sur 12 000 ticks, période **4 003 ticks**. La boucle
d'attract fonctionne. Le titre reste 1 800 ticks (30 s à 60 Hz) en état 1,
puis passe en état 2.

## Ce que fait START

Avec START au tick 3000, et **rien d'autre de changé** :

```text
tick 2452  titre 0x2E3D0100 state 1
(plus rien jusqu'au tick 12 000)
```

L'appui n'est pas ignoré : il **annule** l'avance. Le run va bien au bout de
ses 12 000 ticks (`max_ticks`, `completed_ticks=12000`).

## Qui produit l'avance, et qui l'empêche

Diff des graphes d'appels indirects des deux runs. Arêtes présentes dans le
run neutre et absentes du run START, au tick de la transition :

```text
tick 4251  0x820E9130 -> 0x820EA238
           0x820EA288 -> 0x820EB490
           0x820EA500 -> 0x8217C890
           0x8217C8B8 -> 0x8218AB98     <- retour dans la famille du titre
tick 4254  0x8218A7F4 -> 0x8218AA30     <- le bras d'état 2 de sub_8218A7A8
```

`0x8218A7F4` est exactement le `bctrl` du bras d'état 2 dans
`CModeTaskTitle::update`. L'avance vient donc du moteur `swg` : le film
ActionScript rappelle le jeu en `0x8218AB98`, et c'est ce rappel qui déplace
l'état.

Arêtes présentes dans le run START et absentes du run neutre, au tick de
l'appui :

```text
tick 3000  0x823231B8 -> 0x820DEA08   (64 fois)
tick 3001  0x820DC224 -> 0x820D32D0
           0x820DC224 -> 0x820D3AC8
           0x820E9130 -> 0x820E9838 / 0x820EA538 / 0x820EA550
                       / 0x820EA598 / 0x820EA6C0
```

Le même dispatcher `0x820E9130` choisit d'autres cibles. Le film **répond** à
l'appui, exécute d'autres gestionnaires de script, et ne rappelle plus jamais
`0x8218AB98`.

## Un risque signalé par le rapport `infos` qui ne s'applique pas ici

`AC6_DEMO_PAL_OPEN_FRONTIERS_20260818.md` recommande de qualifier
`0x820D32D0` comme entrée intérieure et avertit qu'en faire une fonction
autonome « rejouerait un prologue déjà exécuté ». Lecture du code généré :

```c
PPC_FUNC_IMPL(__imp__sub_820D32D0) {
	PPC_FUNC_PROLOGUE();
	ctx.r12.u64 = PPC_LOAD_U32(ctx.r3.u32 + 0);
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r12.u32 + 112);
	ctx.ctr.u64 = ctx.r11.u64;
	PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32);
	return;
}
```

`0x820D3230..0x820D3363` est une **table de tail-thunks de 16 octets**, chacun
« charge la vtable, appelle un slot, revient ». Il n'y a pas de prologue à
rejouer, pas de non-volatile à restaurer, pas d'ABI inventée. L'avertissement
est juste en général et sans objet sur cette adresse-ci.

## Non établi

- Pourquoi le film cesse de rappeler `0x8218AB98` après l'appui. Deux lectures
  restent ouvertes : le script attend une ressource que le port ne fournit
  pas, ou il a bien changé d'écran et c'est l'écran suivant qui ne se
  construit pas.
- Le lecteur `CSwgRenderer` affiche toujours `frame=0 of 0` et l'octet de pas
  de `CSwgCallback+9` reste nul dans les deux runs ; cela n'a donc **pas**
  empêché l'avance neutre, et mon commit précédent avait tort de le présenter
  comme le verrou.

# Qui lève l'événement (17, 6) : `CX360UnitManager`, slot `+0x14`

Date : 2026-08-18
Suite de `reports/AC6_DEMO_RENDER_GATE_CROSSCHECK.md`

## La recherche

Le rapport importé établit que la porte `device+0x5460` n'est armée que par le
callback `0x821ADAB8`, sur l'événement `(17, 6)`. Restait à trouver qui lève
cet événement.

Balayage de l'image entière pour la seule forme qui puisse le produire — un
`li rX,17` suivi à moins de huit instructions d'un `li rY,6` :

```text
candidats dans toute l'image : 1
```

Un seul : **`0x820A4778`**, dans la fonction `0x820A45E0`.

## Ce qu'il construit

```powerpc
0x820A4778  li   r11,17        ; 0x39600011
            stw  r11,0x98(r31)
            li   r11,13
            stw  r11,0x9C(r31)
            addi r11,r9,0x4138
            stw  r11,0xA0(r31)
            li   r11,6         ; 0x39600006
            stw  r11,0xA4(r31)
```

Un descripteur portant `event = 17` en `+0x98` et `channel = 6` en `+0xA4`,
avec un `13` et un pointeur entre les deux. C'est exactement la paire que
`0x821ADAB8` compare (`cmplwi r3,17` puis `cmplwi r4,6`).

## À qui appartient la fonction

```text
tools/whose_vtable.py .build/Default.xex.base.bin 0x820A45E0
    at 0x82000CC4   vtable 0x82000CB0 slot +0x14   CX360UnitManager  [RTTI]
```

L'armement du renderer est donc levé par une **méthode virtuelle du
gestionnaire d'unités**, pas par une couche graphique. Le même
`CX360UnitManager` que la pile de patches oracle borne en `0x8226FEC0`.

## État

| maillon | chez nous | oracle |
|---|---|---|
| `0x820A45E0` lève `(17,6)` | **non atteinte** | exécutée |
| `0x821ADC78` enregistre le callback | **atteinte** | exécutée |
| `0x821ADAB8` arme `device+0x5460` | non atteinte | exécutée |
| `sub_821C57D0` teste la porte, 5 463 fois | atteinte | exécutée |

Le seul maillon rompu est le premier. La chaîne complète, du pixel noir à sa
cause, tient maintenant en une phrase : `CX360UnitManager` n'appelle jamais sa
méthode `+0x14`, donc l'événement `(17, 6)` n'est jamais levé, donc la porte
reste nulle, donc la fonction par trame retourne aussitôt, 5 463 fois.

## Non établi

- Pourquoi le slot `+0x14` n'est pas appelé : `0x820A45E0` n'a aucun appelant
  statique, donc l'appel est virtuel et son site reste à trouver.
- Ce que valent les champs `13` et le pointeur `+0x4138` du descripteur.

## Le sous-système entier est hors de portée, pas un appel

Les sept fonctions de l'image qui construisent le déplacement `3248`
(`0x0CB0`, celui de la vtable `CX360UnitManager` en `0x82000CB0`) :

```text
0x82093840  0x82095958  0x82099F20
0x82174888  0x82176930  0x8217C0E8  0x8217C258
```

**Aucune n'est atteinte** par la route par défaut ; **toutes** sont exécutées
par l'oracle. Ce n'est donc pas un appel virtuel manquant : l'instance de
`CX360UnitManager` n'existe jamais dans ce runtime.

## Ce que cela apporte à la classification de portée

`reports/AC6_DEMO_RETAIL_SCOPE_CORRECTION.md` distingue à juste titre
« compilé dans le XEX » de « atteignable depuis le parcours DemoOffline
qualifié », et met en garde contre la confusion des deux.

L'ensemble exécuté par l'oracle tranche cette question de manière
opérationnelle : **l'oracle a exécuté le XEX de la démo**, donc tout ce qu'il
exécute est `demo-active` par construction, quelle que soit la portée du
parcours actuellement atteint par le port. `CX360UnitManager` en fait partie.
Ce n'est pas une ancre retail à conserver de côté : c'est du code que la démo
elle-même exécute et que ce port n'atteint pas encore.

## Divergence de méthode, notée

`reports/AC6_DEMO_PAL_OPEN_BOUNDARIES_NEXT.md` propose de fermer le producteur
par une capture au point d'arrêt sur `0x821ADAB8` (r3, r4, LR, pile, tick).
Les cycles 1603 à 1605 ont établi qu'aucun débogueur guest n'est disponible
sur Xenia Linux — `Stack walker unimplemented on posix`, `--debug` désactivé.
Cette capture n'est donc pas réalisable en l'état, alors que la recherche
statique menée ici a nommé le lève-événement sans elle.

## La porte du renderer appartient au sous-système de mission

En remontant les trois sites de construction, les chaînes se terminent toutes
en appel indirect, mais leurs propriétaires se nomment :

```text
tools/whose_vtable.py .build/Default.xex.base.bin 0x8217B668 0x82093840
    0x8217B668  vtable 0x8200D24C slot +0x00
                CX360MissionManager<CAce6MissionManagerReplay>
    0x82093840  vtable 0x82000CB0 slot +0x00
                CX360UnitManager
```

La chaîne complète est donc :

```text
CX360MissionManager  ->  CX360UnitManager  ->  événement (17, 6)
                     ->  callback 0x821ADAB8  ->  device+0x5460 = 1
                     ->  sub_821C57D0 soumet enfin
```

**La porte de soumission du renderer est une ressource de mission.** Elle
n'est pas armée par une couche graphique ni par un service système : elle
l'est quand le gestionnaire de mission construit son gestionnaire d'unités.

## Les deux fronts n'en font qu'un

Cette session poursuivait deux frontières comme si elles étaient
indépendantes :

- le **frontend** — la boucle d'attract démarrage ↔ titre, que START arrête
  sans la faire avancer ;
- le **rendu** — l'écran noir, la porte jamais armée.

Elles sont le même front. Le rendu ne s'arme qu'en mission ; la mission
n'arrive qu'après le titre ; le titre n'avance qu'avec START. Il n'y a donc
pas deux problèmes à résoudre en parallèle, mais un seul chemin à ouvrir, et
son premier verrou est celui que `reports/AC6_DEMO_START_DURING_TITLE.md`
décrit.

## Conséquence sur la gate Phase 1

La gate attend « un état de menu **et** sa sortie Xenos jointe ». Si la porte
de soumission est de portée mission, alors exiger un readback non noir depuis
l'écran-titre pourrait demander quelque chose que cette route ne produit pas.
Ce n'est pas démontré ici — l'écran-titre affiche bien quelque chose sur
console — mais la possibilité doit être vérifiée avant d'attribuer le noir à
un défaut du port.

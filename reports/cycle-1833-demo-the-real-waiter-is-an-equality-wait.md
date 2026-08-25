# Cycle 1833 — le vrai waiter est une attente d'égalité sur un compteur, pas une fence d'horodatage

## Qualification

Étude **statique d'abord**, sur le code généré du manifeste
`465c279f52993216`, puis une seule mesure bon marché : le watcher générique
existant `AC6_DEMO_WATCH_ADDR_LO/HI`, **sans une ligne de code nouvelle**, run
headless borné à 260 ticks. Aucun oracle.

## Ce que la statique a donné

La base de la structure d'attente n'avait jamais été établie ; elle l'est
maintenant, et **entièrement statiquement** :

```
sub_822DA9C0 :  r31 = 0x82930000 + 18048 = 0x82934680
                r3  = r31 + 136          = 0x82934708     <- objet, global fixe
                sub_822E40E8(r3)

sub_822E40E8 :  r29 = r31 + 64  = 0x82934748              <- objet d'attente
                r5  = [r31+16]  = [0x82934718]            <- « cible »
                sub_822E4018(r3=0x82934748, r5=cible)
```

Aucune inférence de base à partir d'un déplacement : l'objet est un global,
lisible dans le code.

## Ce que la mesure a donné, et ce qu'elle démolit

Sur la fenêtre surveillée `[0x82934700, 0x82934800)` :

```
cible  0x82934718 : écrite 2 fois, toutes deux au tick 0, toutes deux à 0
fence  0x82934758 : écrite 284 fois, ticks 0..259
```

La « cible » vaut **0**. Une comparaison `fence >= 0` est vraie par
construction : sur cet objet, `sub_822E4018` ne peut jamais attendre.

Et le champ `+16` n'est ni un drapeau ni un horodatage. Sa séquence observée :

```
t1 0x0 | t12 0x1 | t1 0x0 | t12 0x1 | t1 0x0 | t12 0x1 0x2 0x3 ... 0x99
```

Le thread 12 publie un **numéro de séquence croissant**, le thread 1 le remet à
zéro. C'est une file producteur/consommateur, pas un rendez-vous temporel.

## Corrections que ce cycle apporte à ses prédécesseurs

- **Cycle 1829, « la fence est un horodatage timebase »** : vrai de l'objet
  `0x82935270` que l'interruption estampille, **faux de l'objet sur lequel le
  guest attend**, qui est `0x82934748`. Les deux sont distincts, séparés de
  `0xB28`. J'avais lu deux objets différents comme un seul.
- **Cycle 1829, « le handshake sauté est `sub_822E4018` »** : `sub_821A6988`,
  le wrapper d'attente, a **trois** appelants — `sub_822E4018`,
  `sub_822E4080`, `sub_822EEE68` — et je m'étais ancré sur le premier sans
  vérifier lequel le thread 1 emprunte.
- Le prédicat de `sub_822EEE68` n'est pas un seuil mais une **égalité** :

```
loop:  acquérir ; r11 = LOAD_U64(obj+16) ; si r11 == attendu -> sortie
       sinon signal+attente, reboucler
```

et son écrivain apparié est `sub_822EEE10(obj, valeur)`, qui fait
`STORE_U64(obj+16, valeur)` — exactement les 284 écritures observées.

## Non établi

- **Que le thread 1 emprunte bien `sub_822EEE68`.** Je le déduis de l'adjacence
  avec l'écrivain et de la forme du prédicat, pas d'une mesure. Les deux autres
  appelants restent possibles, et c'est la première chose à trancher.
- Pourquoi, sous HSIO=1, le thread 1 cesse d'appeler le wrapper d'attente au
  tick 177 alors que la séquence producteur/consommateur, elle, continue
  jusqu'au tick 259 au moins. La lecture qui se présente — le thread 1 trouve
  la valeur déjà égale à celle qu'il attend, ne bloque donc jamais, et épuise
  sa tranche — est cohérente avec les 2998 épuisements, mais elle n'est pas
  mesurée.
- Le rôle du thread 12 et la sémantique des numéros de séquence.

## État

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

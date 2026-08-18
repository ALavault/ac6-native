# L'oracle commence les fonctions ailleurs — correction de `92f76265`

Date : 2026-08-18

## Ce que j'ai publié hier soir

> « Onze fonctions D3D **absentes du port** […] `.pdata` ne les déclare pas,
> donc XenonRecomp ne les émet pas. »

La moitié de cette phrase est fausse. `.pdata` ne les déclare pas, c'est exact.
Mais « absentes du port » ne l'est pas : le code **y est**, sous une autre
adresse de début.

## La mesure

`tools/probe_missing_function_extent.py` propose une étendue ou refuse, selon
les quatre règles que l'expansion de bornes a déjà coûtées. Sur les onze :

```text
0x821A379C  size=8       0x821A59E4  size=108    0x821A59F0  size=96
0x821A7160  REFUS : engloberait la borne déclarée 0x821A7320
0x821ABAE0  REFUS : engloberait 0x821ABAE8
0x821AC5F8  REFUS : engloberait 0x821AC618
0x821CD060  REFUS : engloberait 0x821CD068
0x821CD118  REFUS : engloberait 0x821CD120
0x821AB1F8  0x821ABDC8  0x821B6BC8  REFUS : pas de blr en 400 instructions
```

Les refus sont la partie utile. Quatre pointent une borne déclarée **8 à 32
octets plus loin** — et ces bornes-là sont générées :

```text
oracle 0x821ABAE0  (delta   8) -> 0x821ABAE8  générée, atteinte 592 968 fois
oracle 0x821CD060  (delta   8) -> 0x821CD068  générée
oracle 0x821CD118  (delta   8) -> 0x821CD120  générée
oracle 0x821AC5F8  (delta  32) -> 0x821AC618  générée
```

Une fonction que le port exécute **592 968 fois** figurait dans ma liste de
fonctions « absentes du port ». Elle n'était absente que de la table indexée
par les débuts de fonction que Xenia a choisis.

C'est précisément ce contre quoi `CLAUDE.md` prévient : *ne jamais laisser les
débuts de fonction d'un oracle primer sur les bornes*.

## L'ampleur de l'artefact

Sur les 502 fonctions D3D « exécutées par l'oracle, jamais par nous » de
`b93f52dd`, combien sont en réalité une fonction générée **et atteinte** dont
le début diffère :

```text
à 16 octets près :  72 / 502   (14 %)
à 32 octets près :  93 / 502   (19 %)
à 64 octets près : 106 / 502   (21 %)
```

Le diff reste utile — il reste 400 à 430 fonctions réellement non atteintes —
mais son chiffre de tête était gonflé d'un septième au moins, et je l'ai publié
sans ce correctif.

## Ce qui survit

- `D3D::ComputeClearColor` (`0x821B6BC8`) reste sans `blr` en 400 instructions
  et sans voisine générée proche : celle-là est vraisemblablement bien absente,
  et son `__vector4` explique pourquoi.
- `0x821AB1F8` et `0x821ABDC8` sont dans le même cas.
- `0x821A379C` (8 octets) et `0x821A59E4` (108 octets) ont une étendue propre.
  Note : `0x821A59F0` tombe **à l'intérieur** de `0x821A59E4` — deux entrées
  d'une même fonction, ce que le sondeur ne détecte pas encore entre deux
  candidats non déclarés.

## Non établi

- Si les 400 restantes contiennent une cause. Rien de neuf ici là-dessus.
- L'étendue des trois fonctions sans `blr` : le sondeur refuse, et c'est la
  bonne réponse tant que la terminaison n'est pas établie autrement.

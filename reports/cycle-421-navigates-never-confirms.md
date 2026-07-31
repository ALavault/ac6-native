# Cycle 421 — le dialogue navigue mais ne valide jamais

## 1. Métrique par région, enfin adaptée

Mesure restreinte à la bande de boutons (`y 590-660`, `x 600-1280`), où un
surlignage occupe une grande part des pixels au lieu de ~2 % de l'image.

| transition | bande | plein écran |
|---|---|---|
| arrivée → **Gauche** | **128.458** | 6.814 |
| Gauche → **A** (valider) | 3.452 | 1.478 |
| A → repos (témoin) | 1.673 | 1.962 |
| arrivée → **B** (valider) | 3.918 | 1.389 |
| B → repos (témoin) | 2.803 | 2.021 |

## 2. Lecture

`Gauche` produit **128**, contre un bruit de repos de 1,7 à 2,8 : la sélection
se déplace, sans ambiguïté possible.

`A` produit 3,45 et `B` 3,92, à comparer à 1,7 et 2,8 de repos. Ces écarts sont
du même ordre que le bruit ; **aucune validation n'a lieu**, avec l'un ou
l'autre bouton.

En plein écran, le déplacement de sélection ne pèse que 6,8 — voilà pourquoi il
avait disparu sous le seuil pendant vingt cycles.

## 3. Le blocage, formulé exactement

Le dialogue de sélection de périphérique **accepte la navigation et refuse la
validation**. Ni A (`0x1000`) ni B, pourtant tous deux délivrés à l'invité avec
les masques exacts (cycle 401), ne déclenchent de transition.

L'hypothèse d'une inversion A/B à la japonaise est **écartée** : les deux
boutons sont sans effet, pas permutés.

## 4. Ce que cela restreint

L'invité distingue donc bien les boutons — il agit sur la croix directionnelle
et pas sur les boutons de face. Ce n'est ni un problème de livraison d'entrée,
ni de cadence, ni de durée d'appui.

La question devient étroite : **que fait l'invité du masque de face** sur cet
écran, alors qu'il traite correctement le masque directionnel du même
`X_INPUT_STATE` ?

C'est le premier point d'attaque depuis longtemps qui soit à la fois précis et
non encore réfuté.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

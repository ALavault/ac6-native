# Cycle 172 — fan-out du post-helper NDXR et sélection de candidats

Date: 2026-07-18 (Europe/Paris)

## Cible et provenance

Cible canonique AC6 Xbox 360 PAL : `default.xex`, target ID
`ac6-xbox360-pal`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`, base
`0x82000000`, projet Ghidra `ace-combat-6`.

Le corps concerné est `0x82102568..0x82102e67`, borné par `.pdata`. La preuve
provient du désassemblage XenonRecomp `sub_82102568` et du dump headless
canonique. Aucun code généré ou source natif n'a été modifié.

## Fan-out statique

`0x82102568` appelle `0x822c20c8` à huit retours distincts :

```text
0x82102934  r8/r9 = scratch + 0xb0 / +0xc0
0x8210299c  r8/r9 = scratch + 0xc0 / +0xd0
0x82102a04  r8/r9 = scratch + 0xd0 / +0xe0
0x82102a6c  r8/r9 = scratch + 0xe0 / +0xf0
0x82102ad4  r8/r9 = scratch + 0xf0 / +0x100
0x82102b3c  r8/r9 = scratch + 0x100 / +0x110
0x82102ba4  r8/r9 = scratch + 0x110 / +0x120
0x82102c0c  r8/r9 = scratch + 0x120 / +0xb0
```

Les autres arguments restent, dans la frame de ce helper :

```text
r3 = scratch candidat (vecteur + facteur)
r4 = scratch vecteur modifiable
r5 = record de 16 octets
r6 = record de 16 octets
r7 = vecteur de référence commun
```

Les adresses `scratch + ...` sont relatives à la frame de
`0x82102568`; elles ne doivent pas être transformées en offsets d'objet du
jeu. Le huitième couple reboucle volontairement sur le premier emplacement.

## Deux niveaux de boucle

Avant cette série de huit appels, le helper initialise un indicateur
`found = 0`, puis parcourt :

- deux valeurs de groupe (`r26 = 0..1`), avec un pas de `40` octets dans le
  tableau de records ;
- deux valeurs de sous-index (`r27 = 0..1`), avec un pas de `8` octets.

La série de huit appels est exécutée pour chaque couple. Le chemin statique
permet donc jusqu'à **32 validations** de `0x822c20c8` par invocation de
`0x82102568`, sans compter les sorties anticipées sur bornes ou pointeurs nuls.

Ce nombre est une propriété du contrôle de flux observé, pas une preuve que les
éléments représentent des coins, cellules, avions ou positions.

## Sélection et sorties

Après chaque retour non nul de `0x822c20c8` :

1. le premier candidat valide active `found` ;
2. les candidats suivants comparent leur quatrième flottant au score déjà
   conservé ;
3. si le nouveau score est retenu, les deux vecteurs scratch sont copiés vers
   les deux sorties externes conservées dans `r30` et `r31` ;
4. le score/facteur reste associé au premier vecteur de sortie, à l'offset
   `+12` dans le contrat de ce chemin.

En l'absence de candidat, le helper retourne `0` sans publier de vecteur
accepté. Sinon il retourne l'indicateur `found` après une vérification finale.
La sortie scalaire du caller (l'argument initial `r6`) est initialisée à
`-1`; sur certains chemins tardifs, les champs `r24+52`, `r24+56` et `r24+60`
peuvent produire un bit supplémentaire. Cette valeur ne doit donc pas être
réduite au booléen `found`.

## Relation avec le lecteur `0x82102148`

Le lecteur appelle `0x82102568` avec ses zones scratch et ses deux records
auxiliaires. Le post-helper :

- revalide les indices décalés et les limites `0..15` ;
- fabrique une famille de candidats vectoriels ;
- appelle jusqu'à 32 fois le prédicat `0x822c20c8` ;
- conserve les deux sorties associées au meilleur score observé ;
- peut mettre à jour séparément le scalaire de sortie.

Le résultat du callback indirect et le résultat de `0x822c2868` ne peuvent donc
pas être considérés comme le résultat final du lecteur. La chaîne minimale
reste :

```text
callback 0x821023a0
    -> 0x822c2868
    -> 0x822c20c8 (validation vectorielle)
    -> 0x82102568 (sélection de candidats)
    -> sorties externes / scalaire
```

Chaque étape doit être conservée dans un harness différentiel distinct.

## Confiance et limites

- `confirmed` : huit call-sites et leurs retours ;
- `confirmed` : boucles `2 x 2` et progression des records `40/8` octets ;
- `confirmed` : sélection par indicateur `found`, comparaison de score et
  copie conditionnelle des deux vecteurs ;
- `confirmed` : initialisation `-1` du scalaire caller et voie de mise à jour
  tardive ;
- `unknown` : classe C++, format de record, unités du score et sémantique
  moteur.

Aucune action humaine, VNC ou run Xenia n'est nécessaire pour cette
qualification statique. La prochaine passe utile est de retrouver les
producteurs des records de `40` et `8` octets, toujours sous identité de
binaire et sans leur attribuer de nom gameplay.


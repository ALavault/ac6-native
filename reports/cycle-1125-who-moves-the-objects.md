# Cycle 1125 — qui déplace les objets : le programme d'ordres, et pas un placement

Date : 2026-08-08. La suite directe du cycle 1124.

## Qualification

- Image : Xbox 360 PAL `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Lecture dans `ghidra-projects-xenon/ac6-xenon` (VMX128), **hors du projet
  canonique**, rien n'y est fusionné.
- Charge utile : nœud racine de scénario Mission 01,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- **Statique et produit natif seuls.** Aucun oracle.

## La réponse

Le cycle 1124 avait montré que les 434 objets naissent à l'origine avec la
transformation identité. Ce qui les déplace est **l'ordre d'étiquette 2**, celui
dont le cycle 1122 avait lu le résolveur sans lire son consommateur. Juste après
l'appel :

```
82295bf0  bl 0x822953f0            ; résoudre la cible monde dans r1+0x50
...
82295c44  addi r11,r1,0x50         ; la cible
82295c48  addi r28,r31,0x50        ; la translation de l'objet agissant
82295c58  lvx128 vr124,r0,r11
82295c60  lvx128 vr0,r0,r28
82295c68  vsubfp128 vr0,vr124,vr0  ; cible − position
```

puis une normalisation complète — `vrsqrtefp` et deux raffinements de Newton —
qui en fait un vecteur unitaire. **C'est une direction de pilotage vers la
cible**, pas un placement.

Que `r31` soit bien l'objet construit se lit deux instructions plus haut : à
`0x82295C0C` il lit `+0xE4`, le champ que `0x820A7070` remplit avec le pointeur
de données de l'enregistrement d'unité (cycle 1124).

**Et c'est une seconde confirmation, indépendante, de la disposition** : le
`+0x50` que le constructeur met à zéro est le même que celui-ci lit comme
position courante, et le même que `0x822953F0` additionne — via le biais `+0x10`
de `0x82270380` — pour une cible ancrée.

## La correction, plus importante que la réponse

En cherchant le déplaceur, une lecture du cycle 1122 s'est effondrée.

Les trois premiers mots du bloc de données d'un `ObjBin` étaient lus par le
parseur natif comme un triplet de position. **Ce n'en est pas un.** Chaque route
qui les atteint passe par l'enregistrement classé en `objet+0x180`, et sur chaque
route les trois sont lus **séparément**, chacun derrière un octet de garde
différent du même bloc :

| mot | consommateur | garde |
| --- | --- | --- |
| `+0x00` | `0x8232E168` | seulement quand l'octet `+0x51` vaut 1 |
| `+0x04` | `0x8229B3A8` | à côté de l'octet `+0x52` |
| `+0x08` | `0x8228FD90` et `0x82293570` | rangé dans `objet+0x314` en décrémentant un compteur en `objet+0x310` |

La dernière ligne suffit à trancher : deux fonctions indépendantes rechargent
`+0x08` dans un compte à rebours. **Aucune coordonnée z ne fait cela.** Et rien
ne les charge ensemble, alors que ce binaire charge ses positions d'un seul
`lvx128`.

## Effet sur le produit natif

Le type dit désormais ce que les données sont : `ScenarioUnitRecord::objects`
devient `obj_scalars`, de type `ScenarioObjScalars{first, second, third}`, avec
les trois consommateurs cités dans l'en-tête.

Le monde natif a toujours besoin de coordonnées, et rien de dérivé n'en fournit.
Elles passent donc par une fonction unique, **`position_placeholder`**, dont le
commentaire dit qu'elle est un choix natif et non une lecture du binaire. Le
mensonge tenait dans un type et se répandait ; il tient maintenant dans une
ligne, nommée.

Aucune valeur ne change : `ctest 24/24`, la porte JF reste verte après remise à
jour de l'empreinte de `retail_mission_state.cpp`.

## Ce que cela n'établit pas

- **Le placement initial.** Les objets naissent à l'origine et les ordres les
  pilotent ; entre les deux, sur 230 unités et 890 positions d'ordre, quelque
  chose doit poser la première position, et cette fonction n'a pas été trouvée.
  Un balayage des écritures vers `+0x50` en rend 797 sur tout le binaire —
  l'offset est trop partagé pour désigner qui que ce soit. Il faudra une prise
  plus étroite qu'un déplacement, et ce cycle ne l'a pas.
- **Ce que sont les trois scalaires.** Leurs consommateurs sont nommés, leurs
  gardes aussi ; leur sens ne l'est pas. `+0x08` se comporte comme une période,
  et cela reste une description de son usage, pas un nom.
- **Le reste de l'ordre d'étiquette 2** après la normalisation : la direction
  unitaire est calculée, ce qu'elle pilote ensuite n'est pas suivi ici.

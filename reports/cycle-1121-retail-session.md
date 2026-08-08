# Cycle 1121 — la Mission 01 se joue, et c'est son script qui la termine

Date : 2026-08-08. Les deux derniers comportements ouverts de JF, et une
correction du cycle 1117.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Charge utile : nœud racine de scénario Mission 01,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- **Statique et produit natif seuls.** Aucun oracle, aucun N3 dépensé.

## `0x8219BDD8` — l'ordre du chargeur, relu sur son code machine

Le cycle 1097 avait décrit ce chargeur par ses effets. Voici sa séquence :

```
8219bf54  bl 0x82249718           ; lire le conteneur
8219bf6c  stw r31,0x264(r28)      ; publier la racine à contexte+0x264
8219bf74  bl 0x82266ef0           ; le consommateur du slot 7
8219bf88  lwz r11,0x8(r11)        ; slot 1
8219bfa4  lhz r11,0x0(r11)        ;   son compte u16
8219bfa8  ...  r11*0x14           ;   fois 0x14
8219bfc4  stw r3,0x5c(r28)        ; -> contexte+0x5C, la table de compteurs
8219bfd4  lwz r11,0xc(r11)        ; slot 2
8219bff0  lbz r11,0x0(r11)        ;   son octet de compte
          ... << 2 ...            ;   fois 4
          stw r3,0x2c(r28)        ; -> contexte+0x2C, les horodatages
          lwz r11,0x18(r11)       ; slot 5
          lbz ...                 ;   son octet de compte
          ... * 0x44 ...
          +0x24 = 0 ; +0x08 = 0xFF ; par entrée
          stw r3,0x58(r28)        ; -> contexte+0x58, le recensement
```

Trois tableaux, trois comptes, tous lus dans la charge utile qui vient d'être
publiée. C'est exactement ce que `build_retail_world` fait, et c'est pourquoi
une session bâtie ainsi n'a besoin d'aucun manifeste : **le manifeste ne
contiendrait rien que le conteneur ne dise déjà**.

## La session

`ac6-native --retail-session CHARGE 1 SORTIE` ne prend pas de manifeste — il n'y
a pas d'argument pour en donner un. Elle ouvre la charge utile, bâtit le monde,
le confie au runtime du produit et fait tourner la boucle de session : entrées,
intégrateur de vol, caméra, HUD, 1 800 pas fixes.

```
retail_session units=230 ticks=1800 advances=6 exhausted_at=1800 completed=4
```

Le joueur n'est pas un index choisi : c'est **le seul enregistrement que le
commutateur de faction `0x820A7420` a classé en catégorie 2**, celle que la
table de 16 entrées réserve au joueur local. Les 230 unités entrent en combat
avec les positions que le conteneur leur donne, pas avec la disposition de
développement `20 + i*5` — `MissionLaunchDefinition` accepte désormais les états
de combat tels quels, et le chemin des manifestes garde son ancien
comportement.

### Ce que la session ne prétend pas

`mission_ready` reste **faux**, et c'est délibéré. Dans ce produit, ce drapeau
signifie « les ressources externes déclarées par la mission se résolvent ». La
session retail n'en déclare aucune, parce que les archives restent hors du
runtime et que la parité visuelle est hors du périmètre de JF. Le rendre vrai
demanderait de fabriquer des enregistrements de ressources ; le test affirme donc
le contraire, explicitement.

Ce que le test affirme à la place, et qui se mesure : la position quitte
l'origine, la caméra suit à l'écart fixe du runtime (`-12`, `+3`, `+12`) et vise
le joueur, et **un manche inversé donne une autre session** — sans quoi
« la session prend les entrées » serait une phrase sur un champ que personne ne
lit.

## Le script mène au débriefing — et rien d'autre ne le fait

Le cycle 1120 a lu le curseur `0x82267370` et prédit, à partir des comptes de
pas 2/1/2/1, six pas exécutés dans l'ordre `(0,0) (0,1) (1,0) (2,0) (2,1) (3,0)`
puis l'épuisement au septième appel. Mesuré :

| contrôle | résultat |
| --- | --- |
| trace exécutée | exactement les six pas prédits, dans l'ordre |
| indépendance de la cadence | même trace à 1, 257 et 300 pas de cadence |
| avances nécessaires | 6 après le coup d'envoi, aux trois cadences |
| épuisement | tick 6, 1 542 et 1 800 respectivement |
| débriefing | `Success`, 4 objectifs sur 4, 0 échec |
| **sans avance du script** | tick 1 800, toujours en jeu, **0 objectif complété** |

La dernière ligne est l'anti-but 3 sous forme d'assertion : une session dont le
script n'avance jamais ne se termine jamais. Rien dans ce produit ne complète un
objectif ; seul le curseur le fait, en quittant la sous-mission.

Et la borne que le curseur utilise est **l'octet de la charge utile**, pas le
compte de la table lue par le parseur natif : les deux sont analysés séparément
et le test exige qu'ils coïncident, sous-mission par sous-mission.

### La cadence, dite plutôt que déguisée

`0x822ED708` garde son avance derrière `contexte+0x820` non nul et les six bits
bas de `+0x124` de l'objet de `0x82268C58`. **Aucun des deux n'est modélisé.** La
cadence appartient donc à l'appelant, l'en-tête le dit, et les trois cadences du
test montrent que le résultat n'en dépend pas.

## La correction du cycle 1117 : d'où vient le rectangle de mission

Le cycle 1117 concluait, pour `mission_area` : « la Mission 01 ne déclare aucun
enregistrement de zone, donc le sélecteur n'en rend aucune et **le repli statique
de retail est le chemin pris** ». La seconde moitié est fausse.

Le pas d'étiquette 0 appelle `FUN_82268B28` lui-même, à `0x8226E2A8`, avec quatre
flottants de son propre enregistrement. **La zone est par sous-mission, et le
sélecteur du slot 6 n'est simplement pas le chemin de cette mission.** Mesuré
sur la charge utile :

| sous-mission | x | z | limite (s) |
| ---: | --- | --- | ---: |
| 0 | −50 000 … 50 000 | −50 000 … 50 000 | 0 |
| 1 | −33 000 … 50 000 | −33 000 … 50 000 | 1 800 |
| 2 | −33 000 … 50 000 | −33 000 … 50 000 | 1 800 |
| 3 | −50 000 … 50 000 | −50 000 … 50 000 | 300 |

Le normaliseur et le prédicat portés au cycle 1118 sont donc bel et bien sur le
chemin de la Mission 01, et la session teste la position du joueur contre le
rectangle de la sous-mission courante — sur x et z seulement, comme
`FUN_82268BA0` le fait.

Trois autres champs du même enregistrement ont un consommateur lu dans
`0x8226E158` et sont analysés sans être interprétés : la limite ci-dessus
(`pfVar3[9]`, remise à `FUN_822562B0`), un mot de drapeaux (`0`, `0`, `0x34`,
`0x251` — et tous les bits que `0x8226E158` teste tombent dedans) et un octet de
code de fin (`0`, `1`, `0`, `2`, comparé à 1 et à 2). **Ce qu'ils déclenchent
n'est pas établi et n'est pas porté** ; les nommer sans les tester serait la
règle plausible que les cycles 1111 et 1113 ont appris à tuer.

## Effet sur JF

```
avant : open=playable_session,mission_completion
après : open=none
```

`ctest 23/23`, une ignorée (Vulkan absent de l'hôte).

## Ce que cela n'établit pas

- **La parité visuelle.** Hors périmètre par décision du goal ; la capture HUD
  est mesurée (écritures de pixels, quatre objectifs, 230 unités actives, joueur
  identique à celui de la trame) et non comparée à une référence.
- **La cadence du script**, ci-dessus.
- **Les effets des 36 gestionnaires d'état**, inchangés depuis le cycle 1119.
- Les deux dettes nommées par le goal — les positions monde (`0x822953F0` dans
  le corpus Xenon) et les 1 619 vtables rejetées — restent ouvertes. Le
  rectangle par sous-mission ci-dessus donne pour la première une échelle
  mesurée : ±50 000 unités monde.

## Les artefacts

| pièce | chemin |
| --- | --- |
| statique (le curseur) | `reports/cycle-1120-submission-script-driver.md` |
| statique (le chargeur, la session) | ce rapport |
| test natif | `reports/mission01-retail/retail-session.json` |
| capture | `reports/mission01-native-captures/jf-retail-session/` |
| dérivation (script) | `reconstruction/ace-combat-6/src/retail_mission_script.cpp` |
| dérivation (session) | `reconstruction/ace-combat-6/src/retail_session.cpp` |

Et les quatre refus vérifiés de la porte, sur des copies du contrat :

```
adresse absente de la dérivation → fail: ... never cites retail address 0xDEADBEE0
dérivation vers du généré        → fail: ... cites generated or recompiled source
passed sans dérivation           → fail: playable_session passed without ['derivation']
artefact modifié après coup      → fail: ... evidence size mismatch: retail-session.json
```

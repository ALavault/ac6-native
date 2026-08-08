# Cycle 1133 — le balayage n'était pas exhaustif : 1 018 magasins indexés hors de vue

Date : 2026-08-08. Cycle autonome. Il corrige le cycle 1132, qui est de moi.

## Qualification

- Image : Xbox 360 PAL `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Lecture dans `ghidra-projects-xenon/ac6-xenon`, **hors du projet canonique**.
- **Statique seul.** Aucun oracle.

## Les quatre fausses pistes, fermées

Le cycle 1132 laissait douze sites « calculés » ou « non résolus ». Une fenêtre
de 45 instructions et le suivi du registre vectoriel les ferment tous :

| site | verdict |
| --- | --- |
| `0x820CDE40`, `0x8212AE54`, `0x8212B044`, `0x821F37B4`, `0x822D8EB0` | la source est un `lvx128` ou un `stvx128` antérieur du même registre : **des copies** |
| `0x82126530`, `0x823C12F8`, `0x823D064C`, `0x823D0E60` | `vmulfp128` : **une mise à l'échelle** d'un vecteur existant |
| `0x820F91D0`, `0x820F9514`, `0x820F95D0` | dans `0x820F9168`, qui commence par `vspltisw v0,0x0` puis `vupkd3d128 vr127,0x4,vr0` — **le déballage d'un vecteur nul**, l'idiome qui matérialise `(0,0,0,1)`. C'est une **initialisation à l'identité**, pas la décompression d'une donnée. |

`vupkd3d128` avait tout d'une piste — c'est l'instruction qui déballe les vecteurs
compressés de D3D — et elle n'en était pas une. Il a suffi de regarder son
opérande.

## La correction

Le cycle 1132 écrivait :

> « Dans tout le code de mission, la position d'un objet n'est jamais créée à
> partir du contenu de la mission. […] Ce n'est pas "je n'ai pas trouvé" : les 65
> sites sont énumérés et classés, et le sous-ensemble mission est fermé. »

**La seconde phrase est fausse.** Le sous-ensemble n'est fermé que pour les
idiomes que l'instrument regarde : `stvx128` à index résolvable, et `stfs` à
déplacement littéral. Mesuré sur les 756 029 instructions décodées :

| idiome de magasin indexé | sites |
| --- | ---: |
| `stfsx` | 552 |
| `stvlx` | 218 |
| `stvewx` | 194 |
| `stvrx` | 54 |
| **total invisible** | **1 018** |

Mille dix-huit écritures dont l'adresse est `base + registre`, et aucun des deux
balayages ne les voit. Une position écrite par `stfsx` avec un déplacement
calculé passe sous les deux.

La conclusion correcte du cycle 1132 est donc plus faible, et c'est celle-ci :

> Parmi les écritures de translation que ces deux balayages atteignent, aucune,
> dans le code de mission, ne crée une position à partir du contenu de la
> mission : toutes la copient ou la composent. Ce que font les 1 018 magasins
> indexés n'est pas su.

## Pourquoi l'écrire plutôt que corriger en silence

Cette série a corrigé trois fois ses prédécesseurs — le repli statique du cycle
1117, la confusion unités/entités des cycles 1124-1125, la constante non lue de
« nés à l'origine ». Un cycle qui corrige les autres et pas lui-même n'aurait
rien prouvé du tout. L'affirmation d'exhaustivité était de moi, elle a tenu
quatre-vingt-dix minutes, et c'est une mesure — pas une relecture — qui l'a
tuée.

## Décision de cycle

L'instrument n'est **pas** étendu aux magasins indexés dans ce cycle. Résoudre
`stfsx` demande de suivre la valeur d'un registre d'index, pas seulement un `li`
constant : c'est un travail de propagation, et le bâcler produirait un quatrième
faux positif après les trois du cycle 1128. La prise est nommée, chiffrée, et
laissée entière.

`analysis/translation-writes.tsv` porte désormais l'avertissement en tête.

`ctest 24/24`, la porte JF reste verte.

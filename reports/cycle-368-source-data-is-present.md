# Cycle 368 — les données sources sont bien présentes : le cycle 367 était une déduction fausse

## 1. Correction d'une déduction, par mesure

Le cycle 367 concluait : « rien n'écrit ces adresses après le chargement, donc la
mémoire était déjà vide au chargement ». C'était une **déduction**, explicitement
signalée comme telle, et elle était **fausse**.

Mesure directe des octets sources au moment du chargement :

```
base=028B7000  first256_nonzero=160/256  ptr=ok
base=03514000  first256_nonzero=175/256  ptr=ok
```

**Les deux planches de glyphes contiennent des données réelles.** Environ deux
tiers d'octets non nuls sur les 256 premiers — un contenu compressé plausible,
pas de la mémoire vierge.

La leçon est la même que celle apprise sur les compteurs à zéro, appliquée cette
fois à un raisonnement : **une déduction, même serrée, doit être mesurée avant
d'orienter le travail.** Le cycle 367 renvoyait l'enquête hors du sous-système
graphique ; c'était une fausse piste.

## 2. Ce que la contradiction devient

Pour ces deux textures, il est maintenant **mesuré** que :

| | |
|---|---|
| données sources au chargement | **présentes** (ici) |
| chargement | réussi, une fois chacune (365) |
| vue d'image liée | réelle, non nulle (364) |
| descripteur, format | identiques aux textures qui marchent (362-363) |
| passe qui les dessine | peint bien l'écran (360) |
| **échantillon** | **zéro** |

**Des données valides entrent, une texture vide sort.** Le défaut est donc dans
le **chemin de chargement/téléversement** — entre la mémoire invitée et l'image
Vulkan — et non en amont.

## 3. Réouverture d'une piste

Le décodage `k_DXT4_5` avait été écarté au cycle 362 au motif que cinq autres
textures du même format s'affichent. Cet argument reste valable **pour le format
en général**, mais il n'exclut pas un défaut dépendant des **dimensions** ou du
**pavage** : `256x256` et `320x180` d'un côté, `64x64`, `224x64`, `208x48`,
`960x264`, `64x720` de l'autre.

`320x180` n'est ni une puissance de deux ni un multiple de la taille de bloc DXT
(4) en hauteur : `180 = 45 x 4` l'est, mais `320x180` reste un cas limite de
pavage. C'est une piste, pas une conclusion.

## 4. Front suivant

Journaliser, dans `LoadTextureDataFromResidentMemoryImpl`, le *load shader*
choisi et les paramètres de pavage pour ces deux textures, et les comparer à
ceux d'une texture qui fonctionne. Le chemin est court et déjà instrumenté.

## 5. Correction annexe, signalée par l'opérateur

La cinématique observée aux cycles 356-359 est **l'introduction de démarrage**,
non une séquence d'attrait déclenchée par l'inactivité. Ma lecture initiale
était juste ; la correction intermédiaire ne l'était pas. Conséquence pratique :
après un cache vidé, **attendre plus longtemps est correct** — l'introduction
dure simplement plus longtemps quand les shaders se recompilent. La note de
protocole « ne jamais laisser l'écran-titre s'inactiver » est **retirée**.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

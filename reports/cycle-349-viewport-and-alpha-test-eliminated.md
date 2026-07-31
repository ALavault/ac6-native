# Cycle 349 — viewport et test alpha écartés ; il reste les textures et le shader

## 1. Mesure

Journal par dessin étendu au viewport calculé, à la plage de profondeur, au
test alpha et au nombre de textures liées après traduction. Passe de contenu de
chaque écran, 60 derniers dessins — **valeurs parfaitement stables**, une seule
ligne distincte par écran :

| | sauvegarde (texte **absent**) | titre (texte **visible**) |
|---|---|---|
| décalage viewport | `0,0` | `0,0` |
| étendue viewport | **`1280x720`** | **`1280x720`** |
| échelle NDC | `1.000, -1.000` | `1.000, -1.000` |
| décalage NDC | `0.000, 0.000` | `0.000, 0.000` |
| test alpha | **désactivé** | **désactivé** |
| fonction alpha | 7 | 7 |
| plage de profondeur | `0.000 .. 1.000` | `0.000 .. 0.500` |
| **textures liées** | **2** | **6** |

## 2. Ce que cela écarte

- **Géométrie hors champ ou viewport dégénéré** : écarté. Le viewport de la
  passe défaillante couvre l'écran entier, identique à celui de la passe qui
  s'affiche, même échelle et même décalage NDC.
- **Test alpha qui tue les fragments** : écarté. Désactivé des deux côtés.

Avec les cycles 347 et 348, la liste initiale de quatre causes se réduit :

| cause candidate | statut |
|---|---|
| cible de rendu / mode EDRAM différents | écarté (cycle 348) |
| géométrie hors viewport | **écarté (ici)** |
| test alpha annulant l'écriture | **écarté (ici)** |
| textures liées absentes ou vides | **ouvert** |
| pixel shader mal traduit | **ouvert** |

## 3. La seule différence mesurée

**La passe défaillante lie 2 textures, celle qui fonctionne en lie 6.**

C'est une différence réelle, mais **ce n'est pas en soi un défaut** : deux
shaders distincts utilisent naturellement des nombres de textures différents.
Elle ne devient un indice que si l'une des deux textures liées est vide ou
absente — ce qui n'est pas mesuré ici.

La plage de profondeur diffère aussi (`0..1` contre `0..0.5`). Sans test de
profondeur mesuré ni cible partagée problématique, rien ne permet d'en faire une
cause ; noté, pas retenu.

## 4. Front suivant

Deux causes restent, et une seule mesure les départage :

1. **Contenu des textures liées** — pour les 2 textures de la passe défaillante,
   journaliser adresse invitée, format et dimensions, et vérifier qu'elles sont
   peuplées. Une planche de glyphes vide rendrait le texte invisible en laissant
   la géométrie s'exécuter, ce qui correspond exactement au tableau observé.
2. **Traduction du pixel shader** `8F1C48BA92C8E43E` — vider le shader traduit
   et vérifier qu'il écrit une couleur non nulle, avec `1899F02DC6758D8F`
   (celui du titre, qui aboutit) comme témoin.

L'ordre importe : (1) est moins cher et, s'il montre une texture vide, rend (2)
inutile.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

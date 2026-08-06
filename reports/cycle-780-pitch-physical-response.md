# Cycle 780 — réponse physique bornée au pitch

Date : 2026-08-04

## Résultat

Une expérience Linux native à variable gameplay unique atteint le HUD de vol
Mission 01 dans la lane `bridge`. Après une fenêtre de contrôle nul, le seul
stimulus gameplay est un pitch positif pendant une seconde :

```text
12:24:05.463  XAM ly=32767
12:24:06.447  XAM ly=0
```

Le joueur `0xB2470000` et son enfant unique `0xB2470100` restent identiques.
Une composante du transform copié de l'enfant vers `player+160` fournit le
contrôle physique discriminant :

| fenêtre | heure | bits | float |
|---|---:|---:|---:|
| nul | 12:24:02.536 | `0x3E6F6339` | 0,233777 |
| nul, juste avant pitch | 12:24:04.885 | `0x3E6D55AC` | 0,231772 |
| pitch maintenu | 12:24:05.756 | `0x3E9A79B9` | 0,301710 |
| 0,29 s après relâchement | 12:24:06.739 | `0x3F267EA4` | 0,650370 |

La dérive nulle est -0,002005 en 2,35 s. Le premier sample pendant le pitch
change de +0,069938 en 0,87 s, puis la réponse continue après relâchement. Les
deux autres composantes d'orientation et la position évoluent dans la même
chaîne. L'égalité runtime des trois premiers mots copiés est directe :
`child+112/+128/+144 == player+144/+160/+176`.

Cela soutient fortement qu'un input analogique produit un effet physique
observable derrière l'écran noir. G8 n'est pas encore qualifié au contrat
complet : les quatre demi-mots lus via `r5=0x82A0ACAC` restent constants et
leur attribution à une commande canonique est rejetée. Le maillon
`XAM -> commande enfant` doit être observé directement avant de qualifier le
contrôleur de l'avion.

## Contrôles

- Nul : trois secondes sans input gameplay avant le pitch, puis trois secondes
  après relâchement.
- Positif : unique axe `ly=32767` pendant une seconde ; aucun roll, yaw,
  throttle, tir ou transition forcée dans la fenêtre gameplay.
- Ownership : même joueur, même enfant, même table `0x82007A10` pendant les
  fenêtres nulle et positive.
- Copie : la désassemblage qualifié de `0x822A6710` copie
  `child+112..+175` vers `player+144..+207`; les mots runtime coïncident.
- Négatif : `r5` et ses demi-mots testés ne répondent pas au pitch. Ils restent
  neutres et ne sont pas nommés contrôleur.

Le monde présenté reste noir avec le HUD. Ce résultat ne localise pas le
premier étage noir du render graph et ne transforme pas la lane `bridge` en
preuve stock.

## Identité et validation

- run : `cycle-780-bridge-pitch-child-command`, 12:20:39–12:25:10
  Europe/Paris, Xvfb privé `:97`, timeout 270 s plus cleanup ;
- lane : `bridge`, interventions déclarées
  `save-dialog-synthesis,force-cvars,fallback-allocator` ;
- timing stock : `ac6_performance_mode=false`, `ac6_unlock_fps=false` ;
- runtime commit `b8b03c7a89dc7f23bcd7844d15aa5080d480bf11`, diff suivi
  `fe46948412b4160bfcfe3afe58d38d91aa825560eea22b13a6c3b0bdab71f9da` ;
- probe non suivi utilisé par le build :
  `e28bc28eb1a8008a4feecb952952b16fdedc39f8ea837e7a5458adbfcff67d67` ;
- exécutable `330ee1eda8d0a61a85d1ab045a756c4027f139f63a5ac02b004c8d3ab6f619ee` ;
- log `bb41df1d479c428fcd4b2f6ea3e63307e9718b7b8b0b8b7477c5b828d4ad9ba8` ;
- workspace commit `442c6dbcd5188fb84b056293a3ce7a000bd20669`, diff suivi avant
  rapport `655522cfaa0e2ce228cebda3ebaf22390ddb19bca6d34cde8012e52268508830` ;
- build sans codegen : succès ; CTest PAL 63/63, quatre skips attendus ;
  corpus gelé 54 fichiers, tree `f42fa2c4…`, inchangé ;
- aucun Xenia. Xvfb étranger `:106` existait avant le run puis s'est terminé
  indépendamment ; Ollama `127.0.0.1:11435` est resté intact.

## Prochain test discriminant

Le slot enfant `+0xC0` pointe sur `0x822270A0`. Cette fonction déplace les
demi-mots live `child+382 -> +380` et `child+538 -> +536` avant un dispatch
virtuel. Les observer directement autour du même pitch permettra d'accepter ou
rejeter ce maillon comme commande enfant, sans lire un pointeur `r5` arbitraire.

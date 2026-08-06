# Cycle 781 — rejet des demi-mots du dispatch enfant comme commande pitch

Date : 2026-08-04

## Résultat

Une expérience Linux native `bridge`, à variable gameplay unique, reproduit le
pitch positif du cycle 780 entre deux fenêtres nulles :

```text
12:35:05.479  XAM ly=32767
12:35:06.546  XAM ly=0
```

Le joueur `0xB2470000`, son enfant unique `0xB2470100` et la table
`0x82007A10` restent stables. Le slot `+0xC0`, fonction exacte
`0x822270A0`, s'exécute avant, pendant et après le pitch. Ses quatre demi-mots
candidats restent cependant tous nuls :

| séquence | heure | fenêtre | `+380` | `+382` | `+536` | `+538` |
|---:|---:|---|---:|---:|---:|---:|
| 160 | 12:35:05.020 | avant pitch | 0 | 0 | 0 | 0 |
| 170 | 12:35:05.859 | pitch | 0 | 0 | 0 | 0 |
| 180 | 12:35:06.825 | après relâchement | 0 | 0 | 0 | 0 |
| 190 | 12:35:07.708 | retour nul | 0 | 0 | 0 | 0 |

À la séquence 240, `+382` vaut brièvement 1 puis est remis à 0 par la fonction,
à 12:35:12.361, soit 5,815 s après le relâchement. Ce front tardif est le
contrôle positif que le probe lit effectivement une valeur non nulle, mais son
absence dans la fenêtre causale rejette ces champs comme commande analogique de
pitch.

La réponse physique du cycle 780 est reproduite sur la composante copiée
`child+128 -> player+160` :

| fenêtre | heure | bits | float |
|---|---:|---:|---:|
| nul | 12:35:04.321 | `0x3E6DD3B4` | 0,232253 |
| nul, juste avant pitch | 12:35:05.018 | `0x3E6D382A` | 0,231660 |
| pitch maintenu | 12:35:05.855 | `0x3EA67812` | 0,325135 |
| après relâchement | 12:35:06.821 | `0x3F28C22B` | 0,659213 |

La dérive nulle est -0,000593 contre +0,093475 au premier sample pendant le
pitch. Les quatre mots de transform échantillonnés sont égaux entre l'enfant et
le joueur à chaque sample. La réponse physique est donc reproductible, mais le
maillon commande canonique reste ouvert et G8 reste `supported_not_qualified`.

## Contrôles et limites

- Variable unique : `ly=32767` pendant 1,067 s ; aucun autre axe ou bouton dans
  la fenêtre de vol.
- Contrôle nul : fenêtres avant et après le pitch.
- Ownership : mêmes adresses joueur/enfant/table sur toutes les fenêtres.
- Contrôle positif du probe : `child+382=1` observé puis consommé plus tard.
- Lane `bridge` : aucune preuve stock de gameplay/scénario.
- Aucun Xenia ; aucun processus étranger touché ; Ollama partagé inchangé.
- Le monde reste noir hors HUD ; le premier étage noir du render graph reste
  ouvert.

## Identité et validation

- run : `cycle-781-bridge-pitch-child-dispatch`,
  12:31:36–12:36:11 Europe/Paris, Xvfb privé `:97`, timeout 270 s ;
- timing stock : `ac6_performance_mode=false`, `ac6_unlock_fps=false` ;
- runtime commit `b8b03c7a89dc7f23bcd7844d15aa5080d480bf11`, diff suivi
  `fe46948412b4160bfcfe3afe58d38d91aa825560eea22b13a6c3b0bdab71f9da` ;
- probe non suivi `5de8189e80bdd9272519f31add2dbfd34e63335f3b6b831621201d272cc5dd35` ;
- exécutable `0d49fe98ba1dbee8b2b80b5bc0544213d7ed348819b7c5907176a658c28c2fd6` ;
- log principal `3dd4fa5abca5faf154f9a83870a77f83308a0f441ade3571eee28cfee836ad5a` ;
- workspace commit `442c6dbcd5188fb84b056293a3ce7a000bd20669`, diff suivi avant rapport
  `4e6c24c65df5db0af1a01bc0a1090ccf90f11bc7abca58895702c7d86bf7ff15` ;
- build sans codegen : succès ; CTest PAL 63/63, quatre skips attendus ;
- corpus gelé : 54 fichiers, tree `f42fa2c4…`, inchangé.

## Prochain test discriminant

Reprendre la chaîne live en amont dans `UpInput` à partir des fonctions déjà
bornées `0x821CE088`, `0x82215140` et `0x82214F88`. Ghidra Bridge doit d'abord
qualifier leurs appels et stores ; le prochain probe ne sera ajouté qu'à un
champ directement relié au stick gauche, sans balayage mémoire ni nouveau run
tant que cette frontière statique n'est pas nommée.

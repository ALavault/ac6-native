# Cycle 779 — ownership joueur, enfant runtime et transform

Date : 2026-08-04

## Résultat

Le run Linux natif borné atteint le HUD de vol de Mission 01 avec le monde
visible noir, dans la lane déclarée `bridge`. Il ferme la frontière d'ownership
ouverte au cycle 778 :

- `CAce6UnitPlayer` est `0xB2470000`, vtable qualifiée `0x820568D4` ;
- sa liste à `+216/+220` contient exactement un enfant, `0xB2470100` ;
- cet enfant porte la table de dispatch `0x82007A10` ; aucun nom de classe
  n'est attribué sans RTTI/COL qualifié ;
- le slot joueur `+0x3C`, fonction exacte `0x822A6710`, s'exécute au sein de
  `UpObj` et observe toujours le même enfant ;
- les mots copiés vers le transform joueur changent naturellement. La
  composante à `player+192` passe de `0xC4FC7CB9` (-2019,90) à
  `0x43DE6C55` (444,85), tandis que les trois mots d'orientation échantillonnés
  changent aussi.

Le joueur, son enfant et leur transform ne sont donc ni absents ni figés
derrière l'écran noir. Ce résultat ne ferme pas G8 : l'échantillonnage
`1..8,300` encadre les inputs mais ne sépare pas le mouvement naturel de
l'effet causal de pitch/roll/trigger.

## Contrôles et frontière

- Contrôle positif ownership : le même census retrouve le joueur exact dans
  les 230 objets du UnitManager et `child_count=1` à chacun des neuf samples.
- Contrôle positif update : `0x822A6710` est observée de la frame 11013 à la
  frame 11528 ; `UpInput`, `UpObj`, `UpCam` et `UpRadio` progressent.
- Contrôle positif input brut : XAM reçoit `ly=32767`, `lx=32767`, bouton
  `0x0100` et `rt=255`, avec retour à zéro après chaque stimulus.
- Contrôle négatif : aucun événement des anciens hooks `0x82329B40` ou
  `0x823046A0`. Leur absence ne contredit pas la progression du transform et
  ne constitue pas un défaut physique.

Ghidra Bridge, sur le projet canonique `ace-combat-6/default.xex`, confirme les
relations ciblées disponibles dans l'export : `0x82229200` et `0x82293C88`
appellent `0x82227B08`; le chemin dérivé `0x82293C28/0x82293C88` construit ou
détruit un sous-objet à `+0xA6C*4 = +10672`. Il ne permet pas d'appliquer cet
offset à l'enfant live `0x82007A10` : l'export n'a pas de catalogue vtable et
les frontières de `0x82227B08`/`0x822A6710` restent tronquées. Aucun export
n'a été régénéré.

## Identité et validation

- run : `cycle-779-bridge-player-child`, 12:00:06–12:04:52 Europe/Paris ;
- lane : `bridge`, interventions déclarées
  `save-dialog-synthesis,force-cvars,fallback-allocator` ;
- timing stock : `ac6_performance_mode=false`, `ac6_unlock_fps=false` ;
- runtime commit `b8b03c7a89dc7f23bcd7844d15aa5080d480bf11`, diff suivi
  `fe46948412b4160bfcfe3afe58d38d91aa825560eea22b13a6c3b0bdab71f9da` ;
- exécutable `f202d0467579d0c477ba913cd240580412618c7e97acb0360476c49934d77a5a` ;
- log `375b12933d9313a97b641395dae5352ceb5e803cacc7da6c450d8e97711e64d8` ;
- workspace commit `442c6dbcd5188fb84b056293a3ce7a000bd20669`, diff suivi avant
  rapport `a55eb58c1080d37d7230aca6e2ca81708e8717ba007138456983a9c1fb94aa0b` ;
- build `AC6_ALLOW_CODEGEN=OFF` : succès ; CTest PAL : 63/63, quatre skips
  attendus ; corpus généré inchangé ;
- Xvfb privé `:97`, timeout normal ; aucun Xenia ; processus étrangers
  Xvfb `:106` et Ollama `127.0.0.1:11435` non touchés.

## Prochain test discriminant

Échantillonner le transform enfant et les champs de commande qualifiés à
cadence bornée autour de chaque front XAM, avec un contrôle nul. Le succès est
un join temporel `XAM non nul -> commande enfant non nulle -> variation
d'orientation/vitesse/position`, suivi du retour à zéro, sans écrire l'état
invité et sans réutiliser `player+10672`.

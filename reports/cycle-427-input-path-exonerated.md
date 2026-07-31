# Cycle 427 — hypothèse réfutée : A et Gauche sont traités à l'identique

## 1. Mesure

Sonde sur `sub_8234D210`, relevé après exécution de l'original, sur
l'emplacement actif `0x8290DE3C` :

| appui | `pressed` (+20) | `repeat` (+36) | `cur` (+28) |
|---|---|---|---|
| **A** (espace) | `0x1000` | `0x1000` | `0x1000` |
| **Gauche** | `0x0004` | `0x0004` | `0x0004` |

`delay = 480`, `interval = 96` — tous deux non nuls, la répétition fonctionne.

## 2. Hypothèse du cycle 426 : réfutée

Je supposais que la navigation lisait `[this+36]` et la validation `[this+20]`,
ce qui aurait expliqué la dissymétrie. **C'est faux.** Les deux boutons
apparaissent dans **les deux champs**, avec exactement la même forme. Aucun
traitement ne les distingue à ce niveau.

Formulée au cycle 426 comme hypothèse explicite et non comme résultat, elle a
été testée au cycle suivant. C'est le régime qui convient ; le cycle 394 avait
laissé vivre la sienne vingt-six cycles.

## 3. Ce qui est désormais innocenté, de bout en bout

| étage | vérifié |
|---|---|
| touche → pilote MnK | oui (masques exacts) |
| pilote → `XamInputGetState` | oui (cycle 401, `result=0`) |
| file de frappes | réparée (cycle 422) |
| `packet_number` | corrigé (cycle 423), non utilisé par ce chemin (425) |
| demi-mot `buttons` `[+72]` | oui |
| fronts d'appui `[+20]` | **oui, A présent** |
| répétition `[+36]` | **oui, A présent** |

La chaîne d'entrée est **entièrement disculpée**, de la touche physique jusqu'aux
deux champs que le menu consulte. A y arrive, correctement, sous les deux
formes.

## 4. Conséquence

La divergence est **au-dessus** de cette couche : dans le code de menu qui lit
`[+20]` / `[+36]` et décide. Il reçoit un front d'appui de A parfaitement formé
et n'en fait rien, là où il agit sur Gauche.

Ce n'est plus un problème d'entrée sous aucune forme. C'est la logique de
l'écran.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

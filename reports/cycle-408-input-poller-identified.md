# Cycle 408 — la routine invitée qui scrute la manette est identifiée

## 1. Sonde ajoutée

`xam_input.cpp` : au retour de `XamInputGetState`, le registre de lien contient
l'adresse de retour dans le code invité recompilé, donc le nom de la routine
appelante. Comptage par adresse, résumé toutes les 600 scrutations, derrière le
cvar `ac6_log_input_callers` (faux par défaut ; cette fonction s'exécute à la
fréquence de trame).

## 2. Mesure

| adresse de retour | scrutations | fonction contenante |
|---|---|---|
| `0x8234D418` | 2999 et croissant | **`sub_8234D3F0`** |
| `0x8234D4DC` | 1, constante | **`sub_8234D478`** |

Attribution des fonctions : la liste des symboles générés donne
`sub_8234D3F0` puis `sub_8234D478` puis `sub_8234D50C` ; chaque adresse de
retour tombe dans l'intervalle de la précédente.

## 3. Le fait qui compte

`sub_8234D3F0` scrute la manette **avant et après** l'arrivée sur l'écran
bloqué, au même rythme et depuis le même site d'appel. Les compteurs continuent
de croître pendant que l'écran est figé (599 → 1199 → 2999).

Donc la différence entre un écran qui fonctionne et celui qui est bloqué
**n'est pas dans la scrutation**. Le même code invité lit la manette dans les
deux cas. Ce qui diffère est en aval : ce que le jeu fait du masque de boutons
une fois lu.

Cela restreint utilement la recherche. Il ne s'agit plus de chercher pourquoi
l'entrée « n'arrive pas » — elle arrive, et la routine qui la lit est nommée.

## 4. Point de départ pour la suite

`sub_8234D3F0` est l'entrée. Le travail est de suivre ce que devient la valeur
lue : quel état elle met à jour, et où la transition attendue est court-circuitée
sur cet écran.

`sub_8234D478`, appelée exactement une fois sur toute l'exécution, est
probablement une initialisation ; à distinguer avant de s'y attarder.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

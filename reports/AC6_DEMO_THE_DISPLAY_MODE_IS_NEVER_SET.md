# Le mode d'affichage n'est jamais posé, et c'est ce qui bloque l'anneau

Date : 2026-08-18

## La chaîne, chaque maillon mesuré

```text
sub_821ACCD0 et sub_821AD7C0     NON ATTEINTES (les 2 seules écrivaines)
  -> [0x827AD2F0] = 0           donc index de switch = 0 - 11 = -11
  -> sub_821AD378               11 863 appels, hors plage 0..8, n'écrit rien
  -> [device+21508] = 0
  -> sub_821B9BC8               47 238 appels, abandonne la publication
  -> pointeur d'écriture d'anneau figé à 25 sur 12 000 ticks
  -> 163 930 écritures de buffer indirect n'atteignent jamais le GPU
```

## Comment chaque maillon a été établi

**Le publieur.** Le seul écrivain de `region=ring_publication` est
`sub_821B9BC8` (LR `0x821B9C80`). Il tourne **47 238 fois**. Sa première garde
est le bit 1 de l'octet `[device+10941]` ; mesuré, il vaut `0x06` — **le bit
est posé**, le chemin est bien entré. La garde suivante est
`if ([device+21508] == 0) goto loc_821B9DA4`, et ce champ est mesuré à 0.

**Le champ.** Balayage exhaustif du déplacement 21508 : 3 sites d'écriture dans
2 fonctions, 7 lecteurs. La seule écrivaine atteinte est `sub_821AD378`,
appelée **11 863 fois** — une fois par présentation.

**Pourquoi elle n'écrit pas.** `sub_821AD378` est un `switch` à neuf cas sur
`[0x827AD2F0] - 11`, avec `bgt loc_821AD73C` au-dessus de 8. Le global vaut
**0**, donc l'index est **-11**, donc la sortie est prise à chacun des 11 863
appels et aucun des trois `stw` n'est exécuté.

**Qui devrait le poser.** Deux écrivains de `0x827AD2F0` dans toute l'image,
`sub_821ACCD0` et `sub_821AD7C0` — **aucune des deux n'est atteinte**, ni en
neutre ni avec START.

`sub_821AD7C0` a déjà une histoire dans cette campagne : un cycle l'avait
appelée « le démarreur » sur une recherche d'adresse à forme unique, et le
commit `a6e4c5cc` avait corrigé cela en la décrivant comme un analyseur qui
dispatche sur des caractères `'k'`, `'a'`, `'c'`, `'d'`, `'f'`, `'g'`. Cette
lecture-ci est cohérente avec la correction : un descripteur de mode
d'affichage se lit caractère par caractère.

## Ce que cela remplace

La campagne poursuivait `device+0x5460` (21600). Ce champ est effectivement à
0, mais `sub_821C57D0` le lit comme **valeur empaquetée**, pas comme garde —
c'est ce que notait déjà `12c5a372`. Le champ qui garde réellement la
publication est **`device+21508` (0x5404)**, son voisin, et sa cause est en
amont, dans un global de mode d'affichage jamais initialisé.

## Non établi

- Pourquoi `sub_821ACCD0` et `sub_821AD7C0` ne sont pas atteintes. C'est la
  prochaine question, et elle est bien posée : deux fonctions nommées, un
  global nommé, une valeur attendue dans 11..19.
- Quelle valeur est correcte pour la démo PAL. Les neuf cas ne sont pas
  décodés ici, et deviner un numéro de mode sans contrôle serait exactement ce
  que cette campagne refuse.
- Si `[device+21508]` a d'autres causes de nullité que le switch hors plage.

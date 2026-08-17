# START pendant l'écran-titre : le guest répond

Date : 2026-08-18
Cible : démo PAL `Default.xex` `de917873…405da8`
Sonde : `probe --until frontend --max-ticks 12000`, route par défaut

## La boucle d'attract

Avec l'horizon porté à 12 000 ticks, la séquence de modes est une boucle
stable d'environ 4 000 ticks :

```text
tick   222  CModeTaskStartUpDemoOffline   0x2E7F0080  vtable 0x8201130C
tick  2429  CModeTaskTitleDemoOffline     0x2E3D0100  vtable 0x820113E4
tick  4255  CModeTaskStartUpDemoOffline   0x2E3E0080
tick  6432  CModeTaskTitleDemoOffline     0x2E3D0100
tick  8258  CModeTaskStartUpDemoOffline   0x2E3E0080
tick 10435  CModeTaskTitleDemoOffline     0x2E3D0100
```

Démarrage et titre alternent indéfiniment. Rien n'en sort.

## L'appui était joué au mauvais moment

L'A/B de la campagne — `run_xam_return_chain_ab.sh`, route `buttons_16` —
injecte START au **tick 252**. À cet instant le mode courant est
`CModeTaskStartUpDemoOffline` ; l'écran-titre n'existe pas avant le tick 2429.
Les cycles 1774 et 1775 ont donc qualifié toute la chaîne d'entrée — brut
`0x10`, normalisé `0x400`, logique `0x10` — en la délivrant à un mode qui ne
l'écoute pas, puis le cycle 1776 a conclu que les consommateurs START
`0x82170FCC` et `0x82185210` n'étaient « jamais atteints ».

Rejoué pendant le titre :

```text
probe --input-at 3000,16,0,0,0,0,0,0,1 --input-at 3001,16,… --input-at 3002,0,…
```

le guest réagit **au tick 3001**, une tick après l'appui, et exécute du code
qu'il n'avait jamais exécuté.

## La frontière ouverte par l'appui

```text
AC6 runtime trap: unqualified guest indirect call
tick=3001 thread=1 lr=0x820DC224 address=0x820D32D0
```

`0x820D32D0` est **exécuté par l'oracle Xenia**, donc du code retail vivant.
Il n'existe pas dans le code généré — `ppc_func_mapping.cpp` n'en contient
aucune entrée — parce que ce n'est pas un début de fonction : l'atlas le place
à l'intérieur de `0x820D3230`, dont l'étendue est `0x820D3230..0x820D3363`.
C'est donc une entrée interne à qualifier, la forme que `qualified_thunk.hpp`
traite déjà pour d'autres adresses.

## Non établi

- Ce que fait `0x820D3230`, et pourquoi le titre y saute au milieu.
- Si qualifier cette entrée suffit à faire avancer le mode, ou si d'autres
  entrées internes suivent immédiatement.
- Le tick 3000 est arbitraire, choisi dans la fenêtre du titre ; le moment
  exact où l'appui est accepté n'est pas borné.

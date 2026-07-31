# Cycle 376 — le chemin n'est pas défectueux : ~3 200 textures l'empruntent et s'affichent

## 1. Mesure

Sur la même exécution que le cycle 375 :

```
passages par LoadTextureDataFromResidentMemoryImpl : rangs 1 -> 3200
lignes du filtre « sept textures de la passe »     : 2
bases de ce filtre effectivement journalisées      : 028B7000, 03514000
```

Environ **3 200 textures** empruntent ce chemin pendant que le jeu rend
correctement son écran-titre, sa cinématique et ses menus.

## 2. Ce que cela tranche

L'hypothèse du cycle 375 — « ce chemin est défectueux, toute texture qui
l'emprunte sort vide » — est **réfutée**. Des milliers de textures le
traversent et s'affichent.

Ce qui reste vrai du cycle 375, et qui n'est pas remis en cause :

- les cinq textures d'art de la passe n'empruntent **jamais** ce chemin ;
- les deux planches de glyphes l'empruntent, et échouent.

Mais la conclusion à en tirer change : le chemin fonctionne pour ~3 200
textures, donc **la cause est propre à ces deux-là**, pas au chemin.

## 3. Où cela laisse l'enquête, honnêtement

Vingt-deux causes éliminées. Toutes les propriétés statiques mesurées de ces
deux textures sont identiques à celles de textures qui rendent — y compris,
maintenant, l'appartenance à un chemin de chargement qui fonctionne pour des
milliers d'autres.

Je n'ai pas de candidat suivant qui soit à la fois nouveau et étayé. Les deux
lectures encore ouvertes sont :

1. **la synchronisation** (cycle 374) — jamais mesurée ; ces deux textures sont
   chargées très tard (#2537, #2548), au moment même où la passe les
   échantillonne, alors que les milliers d'autres sont chargées bien avant leur
   usage. C'est la seule différence **temporelle** relevée, et elle n'a jamais
   été testée ;
2. une propriété non encore instrumentée, dont je ne peux pas dire laquelle.

La première mérite d'être testée avant toute autre : elle est cohérente avec le
fait que le chemin marche pour des milliers de textures **chargées tôt** et
échoue pour deux **chargées au dernier moment**.

## 4. Front suivant

Mesurer l'écart entre le rang de chargement et le rang du premier
échantillonnage, pour ces deux textures et pour un échantillon de textures qui
rendent. Si l'écart est nul ou négatif pour les deux fautives et large pour les
autres, la synchronisation devient la cause probable.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

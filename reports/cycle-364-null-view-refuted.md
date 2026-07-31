# Cycle 364 — la vue nulle est réfutée pour nos textures

## 1. Mesure, avec ses deux canaux prouvés vivants

Sonde posée dans `GetActiveBindingOrNullImageView`, journalisant **les deux**
branches — liaisons réussies et vues nulles — précisément pour qu'un compte nul
signifie quelque chose.

```
liaisons réussies : 43
vues nulles       : 63
```

Les deux canaux sont vivants. Les vues nulles **existent** dans ce runtime.

## 2. Résultat

Adresses de base recevant une vue nulle :

```
26x  base=18962000      4x  base=18CD0000      4x  base=189A8000
 3x  base=18C52000      3x  base=18C48000      2x  base=18B32000
```

**Aucune n'est `03514000` ni `028B7000`** — les deux planches de glyphes du
cycle 362. Elles sont toutes dans une plage `0x18xxxxxx` sans rapport.

**Nos deux textures reçoivent donc une vraie vue d'image, et échantillonnent
malgré tout à zéro.** L'hypothèse du cycle 363 est **réfutée**.

## 3. Ce que le bilan devient

Écartés pour ces deux textures : cible de rendu et mode EDRAM, géométrie hors
viewport, test alpha, descripteur absent ou nul, chargement échoué, mémoire non
peuplée, format de sommet, couleur de sommet, décodage `k_DXT4_5`, swizzle et
canal échantillonné, tous les champs de la constante de fetch, et maintenant la
vue nulle.

Ce qui subsiste, non mesuré :

1. la **résidence** — la vue existe mais l'image qu'elle décrit n'a jamais reçu
   ses données pour ces deux textures précises ;
2. l'**invalidation** — les données sont écrites par l'invité *après* le
   téléversement et le cache ne le voit pas ;
3. les **coordonnées de texture** `r0.xy` de la passe, jamais vérifiées, qui
   pourraient échantillonner hors des données utiles.

Le point 3 n'a jamais été testé et redevient le moins cher : le même forçage que
pour la couleur de sommet, appliqué aux coordonnées, le tranche.

## 4. Note de méthode

Cette sonde a été écrite dès le départ avec ses deux branches journalisées. Sans
cela, « 0 vue nulle sur nos bases » aurait été indiscernable d'une sonde morte —
l'erreur commise six fois plus tôt dans l'enquête. Ici le résultat est
interprétable **parce que** le contrôle était intégré, pas ajouté après coup.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

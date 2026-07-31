# Cycle 357 — le diagnostic fonctionne ; sa portée est fausse

## 1. Test tenté

Suite du cycle 356, qui a déduit que l'échantillon de texture `r0` est le
facteur nul : forcer tout échantillon de texture à 1.0 et voir si la couche
d'interface réapparaît en aplats blancs.

## 2. Résultat : écran blanc

```
image 1280x720, moyenne RVB [173.2, 179.1, 173.0], 26 couleurs distinctes
```

L'écran entier vire au blanc. Le diagnostic **fonctionne** — et c'en est la
preuve : la traduction a bien été refaite et le forçage s'applique. Mais il
s'applique **à tous les shaders**, y compris la passe de présentation
(`0A6D1DD7767FDF27 / 2E372EA28CC404B7`) qui échantillonne le tampon de trame
pour l'afficher. Forcer cet échantillon à 1.0 blanchit l'image finale, quel que
soit son contenu.

**Le test est donc invalide par construction.** Il ne dit rien de la couche
d'interface : il masque tout.

## 3. Ce que cela apprend quand même

Deux acquis, l'un méthodologique, l'autre technique :

- le mécanisme de diagnostic par cvar dans le traducteur SPIR-V **est
  opérationnel**, avec le protocole cache-froid puis cache-chaud du cycle 356 ;
- il doit être **restreint au shader visé**. Le traducteur connaît le hachage
  du shader en cours de traduction ; la condition doit être
  `ucode_data_hash() == 0x8F1C48BA92C8E43E`, non un forçage global.

C'est la même erreur de portée que le cycle 347, où `ps=0` semblait significatif
avant que le témoin ne montre qu'il dominait aussi l'écran qui fonctionne : une
mesure trop large ne distingue rien.

## 4. Front suivant, précis

1. Restreindre les deux diagnostics au hachage `8F1C48BA92C8E43E`.
2. Rejouer avec le protocole du cycle 356 : `rm -rf build-rt/cache`, exécution
   de chauffe de 115 s, puis exécution de test avec entrée à 33 s.
3. Si la couche apparaît en aplats blancs, la faute est dans l'échantillonnage
   ou le décodage `k_DXT4_5` ; sinon, la perte n'est pas dans le produit
   `texture x couleur` et il faut remonter à la géométrie de la passe.

Le correctif diagnostique, tel quel, est conservé dans
`patches/rexglue-shader-diagnostics-20260731.patch` — **à ne pas utiliser sans
la restriction par hachage**.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

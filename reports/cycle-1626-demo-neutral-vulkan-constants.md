# Cycle 1626 — constantes Vulkan neutral

## Résultat

Les deux rectangles atteints disposent maintenant chacun d'un descriptor set
Vulkan complet de cinq buffers. Le builder reprend l'ABI ReXGlue générique et
travaille exclusivement sur le snapshot de registres démo joint au draw.

L'analyse statique des trois SPIR-V temporaires qualifie les seuls membres du
buffer système effectivement lus : `0,1,2,3,4,8,9,10,12` pour les vertex
shaders et `0,19,23` pour le pixel shader. Ils correspondent aux flags, index,
NDC, alpha-ref et biais couleur. Aucun float, bool ou loop constant n'est lu ;
les deux bindings float restent les buffers dummy valides de 16 octets exigés
par ReXGlue. Bool/loop (160 octets) et fetch (768 octets) sont des copies exactes
du snapshot de registres.

Les flags système sont scellés par tests : rectangle normal `0x00074B00`,
rectangle copy `0x00070B00`. La taille système est 504 octets. Chaque groupe de
cinq buffers et son descriptor set est publié seulement après allocations,
uploads bornés et mise à jour complète réussis.

Deux runs neutral Vulkan depuis stores frais produisent dix descripteurs de
constantes et restent byte-identiques : RTPLY `c5357c6d…c5794`, rapport
`04116bf6…de35`. Aucun draw, resolve produit ou readback n'est encore soumis.
START n'a pas été exécuté.

## Qualification

- `xenia-generic` : layout ReXGlue, calcul viewport/NDC et règles des flags ;
- `demo-observed` : snapshots PM4 et microcodes des deux draws ;
- `demo-qualified` : membres SPIR-V lus, payloads et dix descripteurs ;
- `unknown` : attachments EDRAM exécutables, commandes draw et pixels.

Les microcodes, désassemblages et SPIR-V sont restés sous `TMPDIR`. Aucun actif
propriétaire, checkout oracle, C++ généré ou projet Ghidra n'est modifié ou
suivi.

Validation : builds codegen OFF/ON, CTest 18/18 et 17/17, deux neutral Vulkan
frais, audit source et complexité : PASS.

## Prochain checkpoint

Créer les attachments EDRAM strictement bornés du rectangle normal, binder les
deux sets et soumettre ce draw uniquement. Vérifier le contenu EDRAM contre
l'oracle CPU avant d'autoriser le rectangle copy et le resolve.

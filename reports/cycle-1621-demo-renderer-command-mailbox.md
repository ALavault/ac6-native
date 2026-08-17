# Cycle 1621 — mailbox transactionnelle du renderer

## Résultat

Le PM4 accepté ne perd plus les entrées nécessaires au renderer produit.
`XenosShaderLoadCommand` conserve désormais les dwords immédiats en mémoire,
en plus du SHA-256 déjà publié. Les draws conservent leurs snapshots immuables
de registres et les identités VS/PS ; le present conserve la ressource bornée.

`GuestBridge` ajoute les commandes à une mailbox bornée à 4 096 éléments
uniquement après validation complète du batch, des écritures guest staged, des
interruptions et des snapshots renderer. `DemoSession` expose un drain par
déplacement, sans copie d'actif vers un rapport ou une trace.

## Frontières

- parser PM4 et transaction : `demo-qualified` ;
- dwords shaders : `demo-observed-ephemeral` ;
- traduction shader runtime et draw Vulkan produit : `unknown`.

Les dwords ne sont jamais sérialisés, installés ou suivis. Les sorties de
rapport restent hash-only. Aucun fallback CPU ou visuel n'est ajouté.

## Validation

- payloads shader runtime identiques aux payloads `IM_LOAD_IMMEDIATE` du test ;
- hashes shader inchangés ;
- snapshots de registres de draws immuables ;
- packets inconnus toujours fatals ;
- build `ccache`, CTest 17/17, audits source/complexité et diff : PASS.

Receipt : `analysis/demo/ac6-demo-renderer-command-mailbox-v1.json`.

## Frontier

Le prochain checkpoint est un adaptateur runtime du translator ReXGlue
épinglé qui consomme exclusivement ces commandes, valide chaque SPIR-V avec
SPIRV-Tools et ne conserve que les modules Vulkan en mémoire. Le premier draw
produit reste fail-closed jusqu'à cette traduction et au pipeline EDRAM exact.

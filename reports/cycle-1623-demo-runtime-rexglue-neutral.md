# Cycle 1623 — translator ReXGlue runtime neutral

## Résultat

Le produit consomme maintenant la mailbox renderer après chaque tick. Le cache
ReXGlue est transactionnel : il conserve les charges shader en mémoire,
traduit au premier draw, valide chaque module SPIR-V et ne remplace son état
qu'après acceptation de toute la séquence. Une identité, une taille, un nombre
de registres, un present ou une référence shader inconnus trappent avant commit.

Sur deux exécutions neutral/headless codegen-ON jusqu'au tick 253, le consumer
observe exactement 5 charges, 26 draws, 1 present et 4 modules uniques. Les
RTPLY restent byte-identiques, SHA-256 `1d41d2e…ebef7`, et les rapports restent
byte-identiques, SHA-256 `d96a9b68…b6a79`. Ils sont également identiques au
gate `rr` antérieur : la traduction n'altère donc ni scheduler, ni PM4, ni
swap, ni replay.

Les quatre sorties éphémères ont été validées avec le `spirv-val` épinglé
`2cc19cdd…e3406`, environnement Vulkan 1.1 avec scalar-block-layout. Seuls les
hashes et tailles sont publiés ; microcodes et SPIR-V restent sous `TMPDIR`.

Le chemin produit Vulkan crée ensuite quatre `VkShaderModule` directement
depuis le cache mémoire. Deux runs Vulkan sur stores frais sont byte-identiques :
RTPLY `c5357c6d…c5794`, rapport `33b6c8b3…85a7`. Aucun pipeline, draw ou
readback n'est encore revendiqué.

Les interfaces des quatre modules sont jointes à l'ABI générique ReXGlue : set
0/binding 0 contient quatre storage buffers de shared memory ; set 1 contient
les cinq uniform buffers system/float-VS/float-PS/bool-loop/fetch. Le produit
exige et active scalar-block-layout, crée deux descriptor-set layouts et un
pipeline layout sans push constant. Les shaders atteints n'utilisent aucun set
texture. Deux exécutions, dont une sur store frais, conservent les hashes
Vulkan ci-dessus.

Deux graphics pipelines sont désormais créés et acceptés par le driver sur
deux exécutions, dont une sur store frais : rectangle normal 640×360, RT
RGBA8 4x MSAA avec depth/stencil D24S8 ; rectangle copy 1280×720, RT RGBA8 1x,
sans depth. Le runtime exige les valeurs exactes observées de
`RB_SURFACE_INFO`, mask, blend, depth, mode et identités VS/PS avant création.
Les RTPLY et rapports restent inchangés. Les pipelines ne sont pas encore
bindés et aucun draw n'est soumis.

## Frontières

- ReXGlue/Xenos : `xenia-generic` ;
- bytes PM4/microcode : `demo-observed` ;
- identités, quatre traductions et validations : `demo-qualified` ;
- modules et layouts Vulkan produit : `demo-qualified` ;
- création des deux pipelines atteints : `demo-qualified` ;
- descriptors alimentés, soumission draw et contenu EDRAM : `unknown`.

Aucune preuve retail n'est fusionnée et aucun checkout, C++ généré, microcode
ou actif propriétaire n'est modifié ou suivi.

Reçu : `analysis/demo/ac6-demo-runtime-rexglue-neutral-v1.json`.

Validation : build codegen OFF/ON, CTest 18/18, deux neutral headless et deux
neutral Vulkan sur stores frais, audits source/complexité : PASS.

## Prochain checkpoint

Alimenter les quatre segments de shared memory et les cinq constant buffers à
partir de la mémoire guest et du snapshot de registres, puis binder et soumettre
le rectangle normal. Le draw reste fail-closed jusqu'à preuve des plages exactes
et du contenu des constantes.

# AC6 cycle 248 — états sampler du contexte texture et hooks corrigés

## Identités

- target : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- projet Ghidra canonique : `ace-combat-6` ;
- checkout AC6Recomp : `.tools/ac6-recomp-reference`, base
  `c5b089fb6988ac504ba394db611543bda2fb2c96`.

Toutes les inspections Ghidra sont headless, en lecture seule et sans analyse
globale. Aucun XEX, projet Ghidra ou fichier généré n'est modifié.

## Contrat du slot texture `+0x2C`

Les implémentations aux adresses `0x8234EFA8` et `0x8234FDE8` appellent
d'abord `0x821E1088` pour publier les six mots du descripteur texture. Elles
appellent ensuite :

- `0x821DC4F8`, qui modifie les bits Xenos `min_filter` et
  `aniso_filter` du dword 3 ainsi que `min_aniso_walk` du dword 4 ;
- `0x821DC688`, qui modifie `mag_filter`, `aniso_filter` et
  `mag_aniso_walk` ;
- pour la variante mip seulement, `0x821DC908`, qui retraduit
  `aniso_filter` lorsque l'un des bits de marche anisotrope est actif et
  mémorise l'enum fourni par stage.

Les noms de champs proviennent du layout vendored
`xenos::xe_gpu_texture_fetch_t`. La table retail `0x82067FA0` fournit la
conversion d'enum vers les valeurs Xenos ; ses premières valeurs sont
`0,0,2,2,3,...`. Les noms précis de chaque enum d'entrée restent inconnus tant
qu'ils ne sont pas joints à leur constante XDK.

Cette frontière qualifie donc un état sampler/fetch. Elle ne qualifie ni une
technique, ni une permutation shader, ni un draw causal.

## Défauts d'instrumentation découverts

Les adresses `0x821DC538` et `0x821DC6C8` sont des chunks remplaçables situés
respectivement dans les vrais corps `0x821DC4F8` et `0x821DC688` :

- avant chaque chunk, le XEX a conservé `device+sampler` dans `r8` ;
- `r4` a déjà été remplacé par `4 * enum_mémorisé` ;
- `r5` contient toujours la valeur demandée.

L'ancien hook `0x821DC538` l'interprétait comme `SetStreamSource`, tandis que
les deux hooks utilisaient `r4` comme index de sampler. Le shadow state était
donc alimenté avec un contrat faux pour ce XEX.

Le correctif source :

- conserve les adresses, symboles et continuations générées ;
- reconstruit `sampler = uint32(r8-r3)` aux deux frontières ;
- classe `0x821DC538` comme chunk `SetSamplerState_MinFilter` ;
- enregistre `min_filter` ou `mag_filter` avec `r5` ;
- ne modifie ni la configuration ni une sortie XenonRecomp.

Fichiers modifiés dans le checkout externe :

- `.tools/ac6-recomp-reference/src/d3d_state.h` ;
- `.tools/ac6-recomp-reference/src/d3d_hooks.cpp`.

Le compteur et le stockage `SetStreamSource` existants sont conservés pour
compatibilité, mais aucun hook qualifié pour cette fonction n'est encore
identifié. Ils ne doivent plus être interprétés comme couverts par
`0x821DC538`.

## Validation

- `VerifyTextureSamplerState.java` passe **29/29** assertions PPC et données ;
- le probe C++23 passe les 16 samplers et un wraparound 32 bits ;
- `d3d_hooks.cpp` passe Clang 21 avec `-fsyntax-only -Wall -Wextra -Werror` ;
- `git diff --check` passe dans le dépôt racine et le checkout externe.

Artefacts :

- `artifacts/ac6-cycle248-texture-slot2c-sampler-audit.log` ;
- `artifacts/ac6-cycle248-sampler-enum-table.log` ;
- `artifacts/ac6-cycle248-texture-sampler-validation.log` ;
- `artifacts/ac6-cycle248-sampler-stage-contract-test.log` ;
- `artifacts/ac6-cycle248-d3d-hooks-syntax.log`.

Le build Linux complet reste bloqué avant génération par l'absence déjà
documentée de `gtk+-3.0` dans `pkg-config`. Aucun run Xenia, VNC ou humain
n'est nécessaire pour la prochaine passe statique.

## Limites et prochaine frontière

Le slot `+0x2C` est désormais qualifié comme bind texture plus configuration
sampler. La jointure MATE vers technique/pass/permutation puis draw reste
ouverte. Avant de l'élargir, il faut auditer les autres chunks D3D configurés
comme fonctions internes afin de ne pas bâtir une trace ordonnée sur des
contrats de registres faux, en commençant par les hooks qui alimentent les
streams et les autres états sampler.

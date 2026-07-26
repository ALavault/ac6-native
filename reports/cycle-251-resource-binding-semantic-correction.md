# AC6 cycle 251 — correction sémantique des bindings de ressources D3D

## Identité et portée

- target : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- projet Ghidra canonique : `ace-combat-6` ;
- mode : `analyzeHeadless -readOnly -noanalysis`, sans Xenia, VNC ou GUI.

Cette passe corrige une conclusion erronée du cycle 249. Les instructions PPC
avaient été correctement relevées, mais les bindings `0x821DD220` et
`0x821DD588` avaient reçu les mauvaises étiquettes stream/index. Le code ne
doit pas pérenniser cette erreur simplement parce qu'elle avait déjà un test.

## Contrats corrigés

### Stream source distinct : `0x821DD068`

Le corps physique reçoit six arguments utiles : device, slot, buffer, offset,
stride et masque de dirty state. Il conserve notamment `r3/r4/r5/r7` dans
`r31/r29/r30/r26`, calcule `device + 0x30A4 + 4*slot`, puis y écrit le buffer à
`0x821DD154`. Des appelants passent des strides `0x10`, `0x20`, `0x30`, `0x40`
et des slots au-delà de trois.

Conclusion : `0x821DD068` est le vrai setter de vertex streams. Il n'est pas
encore présent dans `[functions]`; aucun hook stream n'est donc revendiqué.
Confiance : `confirmed` pour le layout et l'ABI, `cross-match` pour le nom XDK.

### Render targets couleur : `0x821DD220/0x821DD258`

Le corps calcule `(slot + 0xC24) * 4`, soit `device + 0x3090 + 4*slot`, et y
écrit `r5`. Le reset `0x821E6DD8..0x821E6E08` appelle la fonction pour les slots
`0..3`. Au chunk configuré `0x821DD258`, `r3/r4/r5` sont encore
device/slot/surface avant que l'instruction remplacée charge `r3` depuis la
surface.

Le hook alimente maintenant `render_targets[slot]` et
`set_render_target_calls`. Confiance : `confirmed` pour la table de quatre
surfaces et les registres ; `cross-match` pour le nom XDK précis.

### Surface depth-stencil : `0x821DD588/0x821DD5C8`

La fonction écrit la surface à `device+0x30A0`, importe ses champs `+0x1C` et
`+0x20`, et compare ensuite cinq surfaces en parallèle : les quatre entrées
`0x3090..0x309C` et la cinquième `0x30A0`, face à leurs états publiés
`0x31B0..0x31C0`. Le reset efface d'abord quatre color targets puis appelle
séparément `0x821DD588(device, 0)`.

Au chunk `0x821DD5C8`, le device et la surface originaux sont conservés dans
`r31/r30`. Le hook alimente maintenant `depth_stencil` et
`set_depth_stencil_calls`. Confiance : `confirmed` pour le champ séparé et les
registres ; `cross-match` pour le nom XDK précis.

Le vrai index-buffer bind a ensuite été confirmé au cycle 252 à
`0x821DD188`, avec publication universelle à `0x821DD20C`. Il n'est pas encore
capturé car aucune de ces deux frontières n'est configurée ; son champ shadow
et son compteur restent donc à zéro.

## Modifications

Dans le checkout AC6Recomp externe uniquement :

- `src/d3d_hooks.cpp` remplace les captures stream/index erronées par les
  captures render-target/depth-stencil qualifiées ;
- `src/d3d_state.h` borne les color targets à quatre, la profondeur restant un
  champ séparé ;
- aucune sortie générée et aucune configuration XenonRecomp n'a été modifiée.

Dans le dépôt :

- `VerifyStreamIndexChunkContracts.java` conserve son ancien nom de fichier
  pour ne pas casser les commandes existantes, mais vérifie désormais les
  trois familles et publie `AC6_RT_DEPTH_*` ;
- le rapport cycle 249 porte une bannière de correction ;
- le README, le handoff et `PROMPTS_FOR_CHAT.md` dirigent les travaux futurs
  vers stream/index/vertex plutôt que vers des RT/depth déjà retrouvés.

## Validation

- `VerifyStreamIndexChunkContracts.java` : **26/26** assertions PPC exactes ;
- `d3d_hooks.cpp` et `ac6_backend_capture_bridge.cpp` : Clang 21 C++23,
  `-fsyntax-only -Wall -Wextra -Werror`, succès ;
- artefacts :
  - `artifacts/ac6-cycle251-resource-binding-validation.log` ;
  - `artifacts/ac6-cycle251-hook-sources-syntax.log` ;
  - `artifacts/ac6-cycle251-field30a4-producer.log` ;
  - `artifacts/ac6-cycle251-reset-adjacent-bindings.log`.

## Archive annoncée

Au moment de cette passe, les seules archives AC6 visibles restent :

- `ac6-entry163-instrumentation-evidence-v1.zip`, SHA-256
  `2b4d94219731ac6b148bf322b8874e358d45e654ca27f337673c2a13e4a9dba8` ;
- `ac6_ordered_draw_hook_map_v1.zip`, SHA-256
  `5c90f515a187447c4f87a5607596f010e96cd0cff5897e39bdb10bff06c3608d` ;
- `ac6_material_bind_xex_boundary_v1.zip`, SHA-256
  `7acf3070750c9e7ac0aebf9fc37d1c3112adf859c946f3ac23d1346aaee5d0f6`.

La nouvelle archive annoncée n'est pas encore visible ; aucune ancienne
archive n'a été présentée comme nouvelle.

## Prochaine frontière

Ajouter de façon reproductible les frontières universelles `0x821DD068` pour
le stream et `0x821DD20C` pour l'index-buffer, puis retrouver la vraie fonction
de bind vertex declaration. Une modification
de `ac6recomp_config.toml` ne devra être faite que si la régénération peut être
exécutée et testée sans modifier manuellement les sorties générées.

# Cycle 891 — radios intégrées au manifeste runtime

La clé `radios` est maintenant reconnue par `MissionManifestPaths`; si elle
est déclarée, `load_runtime` la valide atomiquement. `--present-manifest`
charge la base et la transmet à `MissionExecution`, qui expose
`dispatch_radio`. Les tables radio restent optionnelles pour les missions qui
n’en ont pas.

Validation : CTest `2/2`, smoke SDL3/Vulkan double-frame sous Xvfb : OK.

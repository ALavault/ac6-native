# Cycle 888 — validation tables optionnelles

`MissionManifestLoader::load_runtime` valide maintenant les tables `input` et
`objectives` lorsqu’elles sont déclarées, sans publier les bases tant que les
trois tables runtime et les options ne sont pas toutes valides. Un manifeste
avec événement input inconnu est rejeté et l’état précédemment chargé reste
intact.

Validation : CTest `2/2`, smoke SDL3/Vulkan double-frame sous Xvfb : OK.

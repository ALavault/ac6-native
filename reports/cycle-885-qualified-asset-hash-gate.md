# Cycle 885 — gate SHA-256 assets qualifiés

Le chemin `MissionManifestLoader::load_runtime` utilise maintenant
`load_qualified_manifest`. Chaque asset consommé par le produit doit porter un
SHA-256 hexadécimal de 64 caractères ; les fixtures courtes restent limitées
aux tests permissifs et sont rejetées par la gate produit. Le chargement reste
atomique.

Validation : CTest `2/2`, smoke SDL3/Vulkan double-frame sous Xvfb : OK.

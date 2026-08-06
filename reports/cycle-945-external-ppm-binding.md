# Cycle 945 — binding PPM externe

Le générateur accepte maintenant `--texture STABLE:PPM`. Il calcule le hash FNV
et la taille du fichier, émet la source dans `textures.tsv`, puis le loader
valide le PPM avant de l’échantillonner.

Validation : un NTXR sky 256×256 décodé par le probe (`swap16`) a été attaché à
`sky000`; `--present-manifest` retourne 0. Les images restent des artefacts
externes et ne constituent pas une preuve de swizzle retail complet.

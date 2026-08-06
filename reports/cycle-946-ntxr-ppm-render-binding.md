# Cycle 946 — NTXR→PPM dans un draw sky

Un NDXR sky réel (`entry119/022_FHM/005_FHM/000_NDXR`) et son NTXR décodé
(`swap16`, PPM externe) ont été chargés ensemble via `--extra` + `--texture`.
Le renderer soumet trois géométries et le readback change par rapport au même
manifeste sans source PPM, preuve que le sample texture atteint la sortie.

La couverture reste clairsemée (`geometry=3`, `triangles=3229`, `writes=2193`,
`coverage=2041`) : transforms sky/caméra et le mapping exact des 13 bindings
restent à fermer avant la gate de parité.

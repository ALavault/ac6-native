# Cycle 899 — liaison NTXR externe fail-closed

Le produit natif accepte désormais une forme étendue de `textures.tsv` :

```text
mission stable texture sampler address fnv64 source_path source_size
```

Les six champs historiques restent compatibles. Pour une tranche NTXR,
le loader résout le chemin relatif au manifeste, vérifie le fichier régulier,
la taille et le FNV-64 exact ; toute divergence est rejetée avant le rendu.

`tools/extract_ntxr_native_slices.py` extrait des fichiers NTXR bornés et écrit
`native-textures.tsv` avec taille, FNV-64, SHA-256, dimensions et format. Un
essai entry 119 a extrait quatre textures 512²/256² et leurs identités.

Ce cycle ne prétend pas encore sampler les texels dans le rasterizer : le
binding matériel MATE/GIDX et le décodage BC1/BC3 restent la prochaine étape.

Validations : manifeste avec tranche NTXR réelle accepté ; CTest normal 3/3 ;
CTest ASan/UBSan 3/3 ; script Python compilé.

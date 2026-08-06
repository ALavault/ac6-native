# Cycle 902 — sampling UV natif dérivé NTXR

Le renderer natif consomme désormais les UV décodés et peut charger une image
PPM dérivée hors ligne d'une tranche NTXR. Le lookup nearest/wrap est appliqué
aux points et aux triangles ; sans image, le fallback matériau reste inchangé.

Sur le smoke caméra bridge, remplacer la texture terrain contractuelle par une
image 512×512 décodée BC3 modifie effectivement la sortie :

```text
baseline  c48ede122a1918a94d9f1269f0c087731696f57378f7de29bb68c57f9fe5b2ac
textured  047f3bcd7ad81de9701507fc5d45dd2ce126f90a1e2771edf7319774e6830336
```

Ce résultat prouve l'influence texel→rasterizer, pas encore le binding MATE/GIDX
retail complet ni la parité couleur oracle. Le PPM est un artefact de manifeste
externe ; aucune texture propriétaire n'est embarquée.

Validations : CTest normal 3/3 et ASan/UBSan 3/3.

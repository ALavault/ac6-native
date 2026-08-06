# Cycle 909 — identité MATE matériau

Les lignes matériau acceptent désormais un `mate_id` optionnel après la
couleur de base :

```text
mission stable shader depth_test depth_write blend base_color mate_id
```

L'identifiant doit être non nul et participe à la signature de shading avec le
`gidx` texture. Les manifests historiques restent compatibles. Un manifeste
Mission 01 sans `mate_id` continue de passer ; une valeur qualifiée peut être
ajoutée sans branche spécifique.

Validations : CTest normal 3/3 et ASan/UBSan 3/3.

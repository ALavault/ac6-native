# Cycle 908 — identité GIDX dans le binding texture

Les lignes texture peuvent maintenant ajouter un `gidx` après les métadonnées
NTXR. Le loader exige un identifiant non nul et le renderer l'intègre à la
signature de shading avec le hash texture, sans branchement par mission.

Forme étendue :

```text
mission stable texture sampler address fnv64 path size width height format gidx
```

Un manifeste avec tranche NTXR 512×512, format 524 et `gidx=268439850` est
accepté. Cela ferme l'identité ressource jusqu'au point MATE/GIDX qualifié ; la
sélection de texture par MATE et les permutations shader restent à joindre.

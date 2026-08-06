# Cycle 895 — readback couleur/profondeur natif

Le chemin `--present-manifest` accepte maintenant deux sorties de vérification :
un PPM RGB et un plan profondeur f32 little-endian. La capture Mission 01
issue du manifeste NDXR borné a produit :

```text
/tmp/ac6-native-m01-20260804-color.ppm  1280x720  2 764 816 octets
/tmp/ac6-native-m01-20260804-depth.f32 1280x720  3 686 400 octets
```

Cette frame reste un smoke natif très sombre : les NDXR sont décodés et soumis,
mais les transforms/caméra, textures et permutations shader ne sont pas encore
raccordés aux constantes de la frame retail qualifiée. Elle n'est donc pas
utilisée comme référence de parité.

Validations après ajout : CTest normal 3/3 et CTest ASan/UBSan 3/3.

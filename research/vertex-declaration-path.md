# Chemin de déclaration vertex NDXR — démo PAL

## Layout sérialisé

La chaîne brute vérifiée sur les 170 NDXR `mapparts` est :

```text
NDXR+0x0A             u16 nombre de records
NDXR+0x30             premier record, stride 0x30
record+0x2A           u16 nombre de descripteurs
record+0x2C           offset du premier descripteur
descriptor            stride 0x30
```

| Offset descripteur | Rôle | Preuve |
|---:|---|---|
| `+0x00` | offset index ; le draw emploie `>>1` | **PROUVÉ** |
| `+0x04` | offset vertex | **PROUVÉ** |
| `+0x0C` | nombre de vertices | **PROUVÉ** |
| `+0x0E` | code format haut | **PROUVÉ** |
| `+0x0F` | code format bas | **PROUVÉ** |
| `+0x10..+0x1C` | quatre offsets matériau | **PROUVÉ** |
| `+0x20` | nombre d'indices 16 bits | **PROUVÉ** |

Les 4 312 descripteurs du corpus démo portent tous `hi=0x06, lo=0x13`.
Le mot vu comme `0x00060613` comprend donc le `u16 vertex_count` puis les deux
octets de format ; ce n'est pas un masque 32 bits unique.

## Consommateur canonique

Dans `Function_822E1560` :

```text
descriptor+0x0E / +0x0F
  -> Function_822EDF60(registry 0x82934970, hi, lo, device, vertex data, 0)
  -> Function_821B0900(...)
```

Ce flux prouve que les deux octets sélectionnent/construisent la déclaration
vertex liée avant le draw. La liste interne `usage/semantic`, le format de
chaque élément, son offset et son stream restent **INCONNUS** : ils sont derrière
`Function_822EDF60`, qui est désormais l'unique frontière statique de ce côté.

## Relation avec le shader

**PROUVÉ pour la classe `0x30000010`** : les mêmes formats `06/13` coexistent
avec `NU_FLAG1=0` et `1`, mais la clé matériau demeure `0x30000010`. Le code de
rendu construit la déclaration depuis le descripteur, tandis que
`Function_822E8668` résout séparément la clé shader du matériau. Il n'existe
donc pas, pour cette classe, de clé recomposée au draw à partir de
`flags | texture_signature | vertex_mask`.

Cela ne réfute pas une sélection combinée pour d'autres familles de matériaux.
La sémantique détaillée de `06/13` reste nécessaire pour les discriminer.


# Cycle 363 — les descripteurs sont identiques ; le suspect devient la vue nulle

## 1. Mesure

Champs restants de la constante de fetch, pour les deux textures fautives
(cycle 362) et trois qui fonctionnent :

| base | dim | swizzle | num_format | endian | tiled | pitch | filtres | rendu |
|---|---|---|---|---|---|---|---|---|
| `028B7000` | 320x180 | `0x688` | 0 | 1 | 1 | 12 | 1/1/2 | **non** |
| `03514000` | 256x256 | `0x688` | 0 | 1 | 1 | **8** | 1/1/2 | **non** |
| `028D0000` | 64x720 | `0x688` | 0 | 1 | 1 | 4 | 1/1/2 | oui |
| `028E9000` | 960x264 | `0x688` | 0 | 1 | 1 | 32 | 1/1/2 | oui |
| `02953000` | 224x64 | `0x688` | 0 | 1 | 1 | **8** | 1/1/2 | oui |

## 2. Ce que cela écarte

**Tous les champs mesurés sont identiques entre les deux groupes.** Swizzle
`0x688` partout, même `num_format`, même endianness, même pavage, mêmes filtres.

Le `pitch` varie, mais **il ne sépare pas** : `03514000` (fautive) et
`02953000` (fonctionnelle) partagent `pitch=8`.

Sont donc écartés : swizzle et canal échantillonné — le principal suspect du
cycle 362 —, `num_format`, endianness, pavage, filtrage, ajustement d'exposant.

**La constante de fetch n'est pas l'endroit où ces deux textures diffèrent.**

## 3. Le suspect restant, et il est nommé

Si le descripteur est identique et que l'échantillon est nul, ce qui est lié
n'est pas la texture décrite. Le cache de textures Vulkan expose exactement
cela :

```
VulkanTextureCache::GetActiveBindingOrNullImageView(uint32_t fetch_constant_index, ...)
```

**Une vue d'image nulle** est liée quand la texture n'est pas disponible. Une
telle liaison échantillonne zéro — sans échec de chargement, sans avertissement,
et sans rien changer au descripteur. Cela réconcilie tout ce qui a été mesuré,
y compris le cycle 352 (« aucun échec de chargement ») : ne jamais devenir
résidente n'est pas échouer à charger.

## 4. Front suivant, à une ligne

Journaliser, dans `GetActiveBindingOrNullImageView`, le cas où la vue nulle est
rendue, avec la constante de fetch et l'adresse de base. Si `03514000` et
`028B7000` y apparaissent et pas les autres, la cause est établie et le travail
se déplace vers **pourquoi ces deux textures ne deviennent pas résidentes**.

Le contrôle de vivacité est obligatoire, comme aux cycles 352 et 359 :
journaliser aussi les liaisons **réussies**, sinon un compte nul ne voudra rien
dire.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

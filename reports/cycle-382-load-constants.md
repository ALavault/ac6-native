# Cycle 382 — les constantes de chargement, et la règle d'alignement

## 1. Mesure

Constantes passées au copieur de blocs 128 bpb, pour les sept textures :

| base | blocs | guest_pitch | lignes | z_stride | host_pitch | rend |
|---|---|---:|---:|---:|---:|---|
| 028B2000 | 16x16 | 32 | 16 | 32 | 256 | oui |
| 028D0000 | 16x180 | 32 | 180 | 192 | 256 | oui |
| 028E9000 | 240x66 | 256 | 66 | 96 | 3840 | oui |
| 0294A000 | 52x12 | 64 | 12 | 32 | 832 | oui |
| 02953000 | 56x16 | 64 | 16 | 32 | 896 | oui |
| **028B7000** | 80x45 | 96 | 45 | 64 | 1280 | **non** |
| **03514000** | **64x64** | **64** | **64** | **64** | 1024 | **non** |

`guest_offset` et `host_offset` valent `0` partout ;
`is_tiled_3d_endian_scale` vaut `0x95` pour les sept.

## 2. Règle déduite

`guest_pitch_aligned` et `guest_z_stride_block_rows_aligned` sont les extensions
en blocs **arrondies au multiple de 32 supérieur** :

```
16 -> 32     52 -> 64     56 -> 64     80 -> 96     240 -> 256
12 -> 32     45 -> 64     66 -> 96    180 -> 192     64 -> 64
```

Vérifiée sur les quatorze valeurs, sans exception.

## 3. Le fait saillant

**`03514000` est la seule texture dont les deux dimensions ne demandent aucun
rembourrage** : 64 blocs de large et 64 lignes de blocs sont exactement deux
tuiles de 32. Sa `guest_pitch` égale son extension, et sa `z_stride` égale sa
hauteur.

C'est la troisième fois que cette texture se distingue par une absence de
rembourrage (cycles 371, 379, ici). Un calcul d'adressage qui suppose
`pitch > extension` — par exemple pour placer une garde ou détecter une fin de
ligne — se comporterait différemment dans ce seul cas.

`028B7000` ne partage pas cette propriété : `80 -> 96` et `45 -> 64` sont tous
deux rembourrés. Sa singularité reste la **hauteur impaire en blocs** (45), la
seule des sept.

Les deux échecs gardent donc des signatures **distinctes**, ce qui continue de
suggérer soit deux défauts, soit un calcul fautif dont ce sont deux cas limites
différents.

## 4. Ce que cela donne à qui reprendra

Les constantes exactes sont maintenant connues pour les sept cas, dont deux
fautifs et cinq témoins. C'est l'entrée directe pour examiner le calcul
d'adressage du copieur 128 bpb : il suffit de le dérouler à la main sur
`64x64 / pitch 64 / z_stride 64` et sur `80x45 / pitch 96 / z_stride 64`, puis
de comparer à `240x66 / pitch 256 / z_stride 96`, qui fonctionne.

Vingt-cinq causes éliminées ; le défaut est localisé, non corrigé.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

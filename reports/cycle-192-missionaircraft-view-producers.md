# AC6 cycle 192 — producteurs des vues `MissionAircraft` et helpers de records

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- image base : `0x82000000`
- projet Ghidra : `ace-combat-6`

Analyse headless, statique et en lecture seule. Aucun projet Ghidra, XEX,
sortie générée, Xenia, Wine ou fichier natif n’a été modifié.

## Producteurs des vues au point `0x8218c810`

Le début du cas de dispatch effectue deux résolutions de nœuds, puis charge
leurs champs `+0x18` :

```text
0x8218ca14: r4  = *(node_a + 0x18)
0x8218ca18: r30 = *(node_b + 0x18)
```

Ces deux pointeurs deviennent les sources de deux descripteurs temporaires :

```text
0x8218ca34..0x8218ca60: init view_a at r1+0x50, source r4 (node_a+0x18)
0x8218ca64..0x8218ca98: init view_b at r1+0x80, source r30 (node_b+0x18)
```

Les descripteurs sont initialisés par `0x82234c18`. Ils contiennent notamment
un compteur, une base, un offset vers la table principale et des tables
auxiliaires. Les cinq appels au service `vtable[+0x14]` suivent cette forme :

| site | index/vue | arguments utiles |
|---|---|---|
| `0x8218cad8` | index `0` des deux vues | `r4=view_a[0]`, `r5=view_b[0]` |
| `0x8218cb04` | index `1` de `view_a` | `r4=view_a[1]`, `r5=0` |
| `0x8218cb30` | index `2` de `view_a` | `r4=view_a[2]`, `r5=0` |
| `0x8218cb5c` | index `3` de `view_a` | `r4=view_a[3]`, `r5=0` |
| `0x8218cba4` | index `4` de `view_a`, table auxiliaire | `r4=view_a[4]`, `r5=0` |

Chaque index passe d’abord par `0x82234dd0`, qui vérifie la borne puis ajoute
un offset relatif à la base de la vue. Le dernier passage vérifie en plus la
table auxiliaire via `0x82234e08` avant l’appel virtuel. La séquence ne montre
aucun accès direct aux champs `CAce6UnitPlayer+0x50/+0x54/+0x58`, au writer
`0x8226f050` ou à un état de caméra/vol.

## Contrats des helpers

### `0x82234c18` — initialisation de vue

`FUN_82234c18(int *descriptor, int record_base)` lit les métadonnées aux
offsets `+4`, `+5` et `+6` du record source, conserve la base, décode un
offset de table et installe un compteur ainsi que plusieurs pointeurs de
tables (`+0xc`, `+0x10`, `+0x18`, `+0x1c` dans la représentation de Ghidra).

Lorsque le marqueur d’endianness n’est pas `1`, il byte-swap le compteur et les
entrées des tables. Le contrat confirmé est donc une construction de vue de
record avec normalisation optionnelle, pas une construction d’objet gameplay.

### `0x82234dd0` — lecture bornée par index

```c
if (index < descriptor->count && descriptor->offsets[index] != 0)
    return descriptor->base + descriptor->offsets[index];
return 0;
```

La fonction ne fait qu’une résolution de pointeur relatif bornée.

### `0x82234e08` et `0x82234e30`

`0x82234e08` lit une entrée de table auxiliaire lorsque l’index est inférieur
au compteur. `0x82234e30` retourne le compteur. Aucun des deux helpers ne
contient de logique de vol, de spawn ou de caméra.

### `0x823330f0` — normalisation inverse de payload

Le seul appel direct recensé est `0x821c1724 -> 0x823330f0`. Le helper parcourt
des éléments et leurs records, efface un champ transitoire (`record+4`),
retire le bit `0x4000` d’un indicateur, convertit des pointeurs absolus en
offsets relatifs et efface le flag de représentation à `+0x20`.

Il s’agit d’une normalisation de représentation de payload, non d’une preuve
que le payload décrit l’avion actif ou la caméra.

## Conclusion sémantique

La chaîne statique est maintenant bornée ainsi :

```text
ResourceManager::MissionAircraft
  -> clé CRC et nœuds hiérarchiques
  -> deux vues issues de node+0x18
  -> lectures bornées des entrées 0..4
  -> service virtuel +0x14
  -> normalisation de records/offsets
```

`MissionAircraft` reste confirmé comme clé de registre de ressources. Le type
concret de ses records, leur consommateur aval et leur éventuelle relation avec
le modèle de vol restent `unknown` / `needs-dynamic-evidence`. Les noms
`resource_view`, `record_entry` et `record_type` sont les plus précis à ce
stade.

## Suite et action humaine

La prochaine piste statique est de suivre les lecteurs des records normalisés
et les producteurs des nœuds résolus, en maintenant la séparation entre
ressources et état gameplay. Aucun run humain, VNC, GUI ou interaction clavier
n’est nécessaire pour ce cycle.


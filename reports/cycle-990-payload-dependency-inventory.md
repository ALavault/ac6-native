# Cycle 990 — inventaire borné des dépendances payloads 9–23

## Provenance et portée

L’inventaire lit les payloads `FHM` décodés par
`tools/extract_ac6_pac.py`, vérifie leur taille et leur SHA-256, puis parcourt
leurs enfants avec des offsets bornés. Il ne copie ni n’embarque de PAC
retail. La provenance est le projet Ghidra `ace-combat-6`, cible
`PAL-default-xex`, module `default.xex`, avec le corpus PAL déjà catalogué.

Commande de génération:

```sh
python3 tools/inventory_ac6_payload_dependencies.py \
  --catalog reports/ac6-pal-campaign-catalog.json \
  --manifest /tmp/ac6-cycle-990-payloads/manifest.json \
  --manifest /tmp/ac6-cycle-986-payloads/manifest.json \
  --manifest /tmp/ac6-cycle-987-payloads/manifest.json \
  --manifest /tmp/ac6-cycle-988-payloads/manifest.json \
  --manifest /tmp/ac6-cycle-989-payloads/manifest.json \
  --output reports/ac6-pal-payload-dependency-inventory.json
```

## Résultat machine-readable

Le catalogue référence
`reports/ac6-pal-payload-dependency-inventory.json`, SHA-256
`4d3ba4ee8e3870d279d19f947c53f06788f9762e67138edb16b5189bfeece122`, taille
6 215 005 octets. La portée couvre les 15 entrées physiques 9–23:

```text
recursive nodes       6778
unique resource hashes 5485
shared resource hashes   52
empty shared hashes       1
```

Chaque ressource conserve son hash, son tag/magic hexadécimal, sa taille,
ses occurrences (`data_table_entry`, chemin FHM, offset, parent) et ses
missions. Les arêtes parent→enfant et les groupes partagés sont également
content-addressed; aucun blob payload n’est publié.

## Formats observés et limites

```text
format  status                              resources  occurrences
MDLP    observed_bounded                           15           15
PLAD    observed_bounded                           15           15
NTXR    observed_bounded                          236          294
NFH     observed_bounded                          235          294
NFIC    observed_bounded                          188          188
Scene   observed_bounded                          176          176
CAPT    observed_bounded                           16           84
NDXR    not_observed_as_standalone_child            0            0
MATE    not_observed_as_standalone_child            0            0
TCAM    not_observed_as_standalone_child            0            0
XMA     not_observed_as_standalone_child            0            0
```

Les contrats `MATE → NDXR → NTXR`, `Scene → TCAM`, objets/unités et
audio/XMA restent explicitement non résolus. `NFIC` et `Scene` sont
inventoriés par bytes et hashes, mais leurs timelines et leur sémantique ne
sont pas supposées. Les missions 3–15 restent donc `partial` pour la
qualification sémantique; aucune ressource inconnue n’est promue dans le
runtime natif.

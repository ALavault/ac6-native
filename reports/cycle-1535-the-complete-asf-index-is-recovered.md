# Cycle 1535 — l'index ASF complet est récupéré

## Résultat

Le point `Header Extension + size` est un ancrage au milieu de la table
d'offsets custom, pas son début. Le lecteur natif remonte maintenant jusqu'à
la frontière bornée unique et récupère les 1 079 mots précédemment omis. Le
cache PAL qualifié expose 6 528 plages : 3 177 dans la banque 0 et 3 351 dans
la banque 1.

Cette correction ne donne toujours aucune signification codec, paquet, frame
ou timestamp aux plages. La lane XMA/ASF M01 reste ouverte sur le demux borné,
la sélection EN/JP, les sous-titres, les événements temporels et la
synchronisation A/V.

## Invariants

- les quatre alignements suivant l'ancrage sont examinés ; un seul candidat
  complet est accepté ;
- suffixe et préfixe sont alignés sur quatre octets, strictement croissants et
  contenus dans la banque ;
- la rupture suffixe n'est un trailer que si son premier mot est inférieur à
  `trailer + 8` ;
- le retour arrière s'arrête uniquement sur un prédécesseur inférieur à cette
  même borne ;
- la sonde, le trailer, le nombre maximal d'entrées, les débordements et les
  frontières ambiguës échouent fermés.

## Contrôle PAL

```text
moviepack.bin sha256 40c28c384beba5cf37eb47d70bcfe99703160278df5670a2f6f56d52b00e6a5a
bank 0 table [0x878,0x3a1c) count 3177 first 37616 last 164634736
bank 1 table [0x8b4,0x3d10) count 3351 first 60132 last 183665040
total ranges 6528
```

Les plages extrêmes absolues sont `[37616,39452)`,
`[164634736,164638720)`, `[164698852,164702004)` et
`[348303760,348307456)`. Les anciennes premières plages sont désormais les
entrées 547 et 532, ce qui pince exactement les deux coutures de la correction.

## Validation

```text
build ac6-retail-content-tests                       pass
test synthétique positif et rejets fail-closed       pass
cache PAL qualifié                                   pass, 3177+3351=6528
git diff --check                                     pass
```

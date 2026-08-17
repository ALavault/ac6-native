# Cycle 1721 — inventaire statique XMA des packs de voix PAL

## Verdict

Les deux fichiers de voix présents dans le store démo PAL ont été inspectés
hors ligne, sans suivre ni copier leur contenu dans le projet. Chaque fichier
est une concaténation de 738 RIFF/WAVE contigus et bornés. Les 738 segments
anglais et les 738 segments japonais s’ouvrent avec `vgmstream-cli` et FFprobe
sans erreur. Les métadonnées échantillonnées identifient XMA1, 48 kHz, mono,
avec des durées et débits propres à chaque segment.

Aucun segment n’a été décodé en PCM, et aucune jointure vers le consumer PAL,
le store `XMACreateContext`, un timestamp, un volume ou un packet runtime n’est
faite. Les noms `voicepack_eng.bin` et `voicepack_jpn.bin` qualifient les
fichiers statiques présents; ils ne prouvent pas encore la route audio choisie
par le jeu.

## Identité et provenance

| élément | valeur |
|---|---|
| cible | `Default.xex`, démo PAL, Xenon big-endian/Xenos |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| racine observée | `demo-game-file/extracted/stfs-root` |
| `Default.xex` local SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| manifeste PAC/STFS SHA-256 | `bf3bc0549c05b4b6de53017a885332fb51bcb682ac769a18674e5b00f9f26496` |
| outil vgmstream | commit `5bf4bdf0de37710b412bd81649e513fb054cef85`, binaire SHA `f88f81c5d6718d611836683d5cce28622d31639b51776a1a90e7307330a0cab5` |
| FFprobe | `/usr/bin/ffprobe`, SHA `a6dac1e9e8631e04075d06854cc4069308aec279bb04683b4a0bca5e4f54b2fe` |
| outil manifeste | `.tools/ac6-extra-tools.json`, SHA `945c05a3b75859c7607f315632b4c83526bda593df0de3871d0455312570570e` |
| extraction de segments | temporaire sous `/fastdata/lavaulta/tmp`, supprimée après contrôle |

## Inventaire borné

| pack | taille | SHA-256 | RIFF valides | longueur RIFF min–max | `data` min–max | ouverture vgmstream | ouverture FFprobe |
|---|---:|---|---:|---:|---:|---:|---:|
| `voicepack_eng.bin` | 16 988 160 | `f482d54d8314c4eae8c3ab20cda40a6c539c96cd933dcfde3c41f03b33e121ba` | 738 | 6 144–92 160 | 4 096–90 112 | 738/738 | 738/738 |
| `voicepack_jpn.bin` | 21 876 736 | `e67c7add7e4dde4297ae87ec0ca30e6e91333d32511b6fd12e8f2d01e4c987b8` | 738 | 10 240–83 968 | 8 192–81 920 | 738/738 | 738/738 |

Chaque segment respecte `RIFF size + 8 == end`, contient `fmt `, `ALIG`,
`x2st` et `data`, et le segment suivant commence exactement à cet `end`; le
parseur n’a rencontré aucun gap non nul ni dépassement de borne. Le tag de
format brut est `0x0165`, confirmé comme `xma1` par FFprobe.

## Métadonnées de contrôle

Les segments d’offset 0, médian et dernier de chaque pack ont tous produit :

- vgmstream : `Xbox Media Audio 1`, `48000 Hz`, `1` canal, layout `flat`,
  type `float`, métadonnées issues du RIFF Microsoft, débits observés de
  88–91 kbps;
- FFprobe : `codec_name=xma1`, `codec_tag=0x0165`, `sample_rate=48000`,
  `channels=1`, `sample_fmt=fltp`.

Les durées échantillonnées vont de 2,679 à 4,545 secondes pour l’anglais et
de 3,500 à 5,062 secondes pour le japonais. Ce sont des valeurs de contrôle
sur les segments échantillonnés, pas une durée agrégée de pack.

## Classification et limites

- **demo-qualified** : hashes des deux fichiers, 738 bornes RIFF par fichier,
  structure de chunks, ouverture 738/738 par les deux outils et tag XMA1.
- **demo-observed** : présence des deux fichiers nommés `eng` et `jpn` dans la
  racine extraite du store démo.
- **xenia-generic** : aucun comportement Xenia importé.
- **unknown** : sélection runtime, liaison aux appels PAL/XMA, packets,
  timestamps, volume, décodage effectif, synchronisation et langue jouée.

La procédure n’a produit aucun WAV/PCM durable et ne suit aucun actif
propriétaire. Elle ne ferme donc pas la lane audio runtime et ne justifie pas
une implémentation de `XMACreateContext`.

## Prochain checkpoint

Conserver l’inventaire comme oracle statique. Qualifier d’abord l’effet du
store PAL `0x7FEA1A80` et le premier consumer avant de relier un segment à un
packet ou d’activer FFmpeg/SDL3 dans `play`.

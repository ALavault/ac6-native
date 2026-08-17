# Cycle 1711 — échantillons des pointeurs XMA

## Verdict

Les six pointeurs non nuls observés dans les trois entrées XMA ont été testés
par un échantillon borné de 64 octets, en neutral et START. Tous sont mappés,
mais tous sont entièrement nuls dans cet échantillon :

```text
SHA-256 échantillon (64 octets) : f5a5fd42d16a20302798ef6ed309979b43003d2320d9f0e8ea9831a92759fb4b
premier mot : 0x00000000
```

Cette preuve ne nomme pas les champs et ne conclut pas qu’aucun flux XMA
n’existe ailleurs. Elle établit seulement qu’aucun payload non nul n’est
atteint dans ces ranges au moment du premier `XMACreateContext`.

## Ranges observées

| entrée | pointeur A | pointeur B | échantillons |
|---:|---|---|---|
| 0 (`0x17360010`) | `0x17360180` | `0x17361F80` | nuls / SHA identique |
| 1 (`0x17360070`) | `0x17362180` | `0x17363F80` | nuls / SHA identique |
| 2 (`0x173600D0`) | `0x17364180` | `0x17365F80` | nuls / SHA identique |

La table reste `0x17360000`, `count=3`, `flags=0x00030000`, `entries=0x17360010`,
stride `0x60`; les slots `entry+0x40` sont nuls et l’entrée 0 déclenche
l’import.

## A/B et identité

- Cible PAL : `Default.xex`, SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.
- Build : `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp`, SHA-256
  `61a0c4848a9f8649dd0cd9979c2caf25d189e9b0f5dff522ed910082c69236fd`.
- Hook `src/guest_bridge/xma_import_trace.hpp` SHA-256
  `b95a74395922f60b409fb7f1e86d0ccba7707df32cec929b9ad200f89a3c780c`.
- Bruts : `/fastdata/lavaulta/tmp/ac6-cycle1711.cWAvnp/`.
- RTPLY neutral/START : `6a759832…f20` / `d53bf82d…c8d`.
- Rapports neutral/START : `7c2555e8…131` / `238bd9e4…2cc`.
- stderr de la sonde : `71cf5631963d6b299246eca750dd1ce4311e9ce3261f8468d1ae4aeb4fcb2d8`, identique A/B.
- 911 PRESENT, ordinal 548 trap au tick 1048/thread 21, identique A/B.

## Classification

- **demo-qualified** : six adresses, mappage, 64-octet samples, SHA et premier
  mot zéro; égalité neutral/START; table et sélection d’entrée précédemment
  jointes.
- **demo-observed** : répétition des deux pointeurs dans chaque entrée.
- **xenia-generic** : aucun élément promu.
- **unknown** : sens des pointeurs, taille réelle, payload après ce sample,
  ABI/retour XMA, packets/timestamps/volume/PCM, pixels et mission.

La capture reste opt-in, lecture seule et sans extraction de média. Le service
audio ne doit pas appeler `vgmstream-cli`/FFmpeg sur cette preuve négative.

## Prochain checkpoint

Cibler les premiers stores/loads guest sur les six ranges exactes pendant une
exécution plus longue ou une route qui dépasse l’import, sans augmenter la
fenêtre de capture ni retirer le trap. Un premier mot non nul et son producteur
PAL sont requis avant toute qualification XMA.

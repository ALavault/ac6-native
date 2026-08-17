# Cycle 1712 — frontière XMA post-import (expérience opt-in)

## Verdict

Une expérience strictement opt-in a franchi l’import PAL
`xboxkrnl.exe:XMACreateContext` ordinal 548 et a retrouvé le premier effet
post-import exact. Le chemin de production reste inchangé et fail-closed :
sans `AC6_DEMO_EXPERIMENTAL_XMA_CREATE=1`, l’import piège toujours au tick
1048. L’expérience n’est donc pas une qualification audio et ne ferme aucune
lane du gate.

## Identité et protocole

| élément | valeur |
|---|---|
| cible | `Default.xex` démo PAL |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| architecture | Xenon big-endian / Xenos |
| basefile SHA-256 | `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| binaire expérimental | `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp` |
| binaire SHA-256 | `d6d957c05c01c70a1bea4f3d90d6863cf531a554e61f4510ab1921add7925016` |
| expérience | `AC6_DEMO_EXPERIMENTAL_XMA_CREATE=1`, callsite/registers PAL exacts uniquement |
| borne | `max_ticks=1050`, stores neufs, neutral et START |

Le chemin opt-in alloue un tableau de 320 contextes de 64 octets, écrit le
pointeur du premier contexte dans `0x17360050`, puis retourne le statut zéro.
Cette allocation vient de l’autorité générique Xenia/ReXGlue et sert seulement
à exposer le prochain accès guest; aucun paquet, timestamp, volume ou PCM n’est
créé.

## Résultat A/B

| route | RTPLY SHA-256 | rapport SHA-256 | stderr SHA-256 | frontier | PRESENT |
|---|---|---|---|---|---:|
| neutral | `6a759832c5471405591dac956bbc96dd99ab087d274067d01eea18849b428f20` | `ca7e9ba7f9c5828d78a37ccbeef2bc3f62f306801a33c59b3f8db24ffc8db855` | `4e2535be3a3a28b382348756035dae167877225988ed4bfb294707148ed6bbf1` | tick 1048, `0x7FEA31E0` | 911 |
| START (`0x10` au tick 252) | `d53bf82d50724f6b0d771f30edfcdc17439b63fabba6a1e884b72a1ac2268c8d` | `5c8360ee11c113123e71d2a06509c08090eb24453d42d5147708dfb136409a6c` | `4e2535be3a3a28b382348756035dae167877225988ed4bfb294707148ed6bbf1` | tick 1048, `0x7FEA31E0` | 911 |

Les deux routes ont le même frontier et le même stderr. L’import opt-in est
atteint avec `LR=0x82357298`, `r3=0x17360050`, `r4=0`, `r5=0x6180`, `r6=0`,
`r7=1`. Le pointeur écrit est `0x2EEEC000`; le `MmGetPhysicalAddress` suivant
est atteint une fois avant le trap.

## Premier effet post-import

Correction de provenance (cycle 1713) : le diagnostic runtime expose
`lr=0x823572AC`, qui est l'adresse de retour du `bl MmGetPhysicalAddress`,
pas le PC de l'instruction qui piège. Le basefile PAL vérifié donne
`0x823572AC: 81 7C A5 2C` (`lwz r11,-23252(r28)`) puis
`0x823572D8: 7D 60 55 2C` (`stwbrx r11,0,r10`) et
`0x823572DC: 7C 00 06 AC` (`eieio`). Le store est donc qualifié
statiquement à `0x823572D8`; le runtime ne fournit ici que le LR
`0x823572AC` au moment du trap.

Les preuves dynamiques donnent :

```text
PC statique du store / LR runtime : 0x823572D8 / 0x823572AC
adresse MMIO : 0x7FEA31E0
valeur logique : 0x00000001
valeur passée par le code généré au store big-endian inversé : 0x01000000
thread/tick : 21 / 1048
```

Le rapport runtime piège sur l’adresse non mappée avant tout effet. La plage
`0x7FEA3100..0x7FEA3FFF` est explicitement inconnue dans la table générique
ReXGlue; le registre dword `0x31E0/4 = 0x0C78` n'a donc pas de nom ou de
sémantique promouvable. Le PC dynamique exact du store et l'effet matériel
restent inconnus au-delà de cette jointure statique.

## Sources et classification

- **demo-qualified** : callsite/registre de l’import, `output_slot=0x17360050`,
  pointeur expérimental observé `0x2EEEC000`, appel `MmGetPhysicalAddress`,
  adresse/PC/tick/thread du premier store MMIO, égalité neutral/START et
  absence de frontend/mission/terminal.
- **xenia-generic** : Xenia `tools/xenia-source`, commit
  `95a5c3ee250f80c3b9d139658649d9ffb6db3eec`,
  `xboxkrnl_audio_xma.cc` SHA
  `73c92f8c5196694f838a6f787750681e3f7a4cd044b478c118c3035a46e3c86c`;
  ReXGlue `cb58065c793429aa92895d778af58d12e9d26d8f`,
  `XMA_CONTEXT_DATA=64` octets, `kContextCount=320`,
  `AllocateContext` écrit un pointeur de sortie et renvoie succès ou
  `X_STATUS_NO_MEMORY`.
- **demo-observed** : écriture opt-in du pointeur et progression jusqu’au
  store MMIO.
- **unknown** : registre `0x0C78`, endian wire effectif côté matériel,
  contexte global/bitmap, `XMAInitializeContext`, packets/timestamps/volume,
  PCM, langue, `vgmstream`/FFmpeg et toute image guest-owned.

## Garde et validation

Le handler expérimental est borné à l’environnement, au LR et aux sept
registres PAL observés dans
`recompilation/ace-combat-6-demo/src/guest_bridge/audio_memory_dispatch.hpp`.
La route par défaut a été rejouée sans l’environnement : elle piège toujours
`unimplemented import xboxkrnl.exe ordinal 548` au tick 1048, 911 PRESENT, sans
marqueur expérimental. Aucun fichier Xenia/ReXGlue, Ghidra, C++ généré,
microcode ou actif propriétaire n’a été modifié ou suivi.

Validations locales :

- build incrémental réussi;
- audit complexité : `pass`, 90 fichiers;
- `ac6-demo-xenos-tiling-tests` et `ac6-demo-vulkan-resolve-tests` : `2/2`;
- aucun readback, décodage audio ou screencap promu.

## Prochain checkpoint

Ne pas mapper `0x7FEA31E0` par approximation. Obtenir une preuve PAL séparée
du registre XMA `0x0C78` (écriture/lecture et effet sur l’entrée), ou interrompre
l’expérience et revenir au trap par défaut. Tant que ce registre et le premier
paquet XMA exact ne sont pas qualifiés, `vgmstream-cli`, FFmpeg, SDL3 audio,
readback et screencap restent interdits.

Capsule : `analysis/demo/ac6-demo-xma-experimental-mmio-frontier-v1.json`.

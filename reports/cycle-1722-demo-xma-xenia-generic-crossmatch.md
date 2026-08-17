# Cycle 1722 — cross-match XMA Xenia générique / démo PAL

## Verdict

Le checkout Xenia épinglé documente une formule générique de sélection de
contexte XMA : une adresse physique de contexte est convertie en index, puis
en registre indexé et en bit mask big-endian. Il appelle cette mécanique avec
les bases `0x1A80` (initialisation), `0x1940` (enable) et `0x1A40` (disable).
Le tableau ReXGlue épinglé fournit, séparément, des noms génériques pour la
plage `0x0600..0x06FF`.

La démo PAL montre une arithmétique binaire analogue dans ses propres bytes et
un store tenté vers `0x7FEA1A80`, mais aucune preuve ne joint cette adresse,
ces bases ou ces noms à un registre matériel AC6. Le cross-match reste donc
informatif uniquement : aucune table XMA n’est installée et l’ordinal 548
reste fail-closed.

## Autorités et hashes

| source | commit | fichier | SHA-256 |
|---|---|---|---|
| Xenia générique | `95a5c3ee250f80c3b9d139658649d9ffb6db3eec` | `src/xenia/kernel/xboxkrnl/xboxkrnl_audio_xma.cc` | `73c92f8c5196694f838a6f787750681e3f7a4cd044b478c118c3035a46e3c86c` |
| ReXGlue générique | `cb58065c793429aa92895d778af58d12e9d26d8f` | `include/rex/audio/xma/register_table.inc` | `f036b553b353a90b18dd7642a233ec0abcb98d372c644259263f55459acbaa7f` |
| démo PAL | — | `Default.xex` | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| basefile PAL | — | `xex-basefile.bin` | `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |

Les deux checkouts génériques sont lus seulement; aucun fichier n’a été
modifié et aucun microcode, shader, audio décodé ou actif propriétaire n’est
suivi.

## Preuve générique

`StoreXmaContextIndexedRegister` (Xenia, lignes 81–93) calcule :

```text
hw_index = (physical_context - context_array_ptr) / sizeof(XMA_CONTEXT_DATA)
reg_num  = base_reg + ((hw_index >> 5) * 4)
reg_value = 1 << (hw_index & 0x1f)
WriteRegister(reg_num, byte_swap(reg_value))
```

Les callsites Xenia sont `XMAInitializeContext_entry` ligne 189 (`0x1A80`),
`XMAEnableContext_entry` lignes 360–365 (`0x1940`) et
`XMADisableContext_entry` lignes 367–380 (`0x1A40`). Le tableau ReXGlue
marque `0x0600` comme `ContextArrayAddress`, `0x0606/0x0607` comme index
courant/suivant et les groupes `Kick`, `Lock`, `Clear` aux bases `0x0650`,
`0x0690`, `0x06A0`; ses commentaires restent explicitement incertains.

## Jointure PAL, sans promotion

Les bytes PAL qualifient indépendamment la formule
`I=((P-G)>>6)&0xffff`, `A=0x7FEA1A80+((I>>5)<<2)`,
`V=1<<(I&0x1f)`, avec lecture à `0x7FEA1800`, global `0x829DA52C`, puis
`stwbrx`/`eieio`. Les traces neutral et START direct/rr observent à tick 1048,
thread 21, `P=0x2E800000`, `A=0x7FEA1A80` et `V_wire=0x01000000`, puis
trapent avant effet.

Cela établit seulement une similitude de forme et une adresse PAL observée.
La coïncidence numérique du suffixe `0x1A80` avec une base Xenia générique ne
nomme pas le registre PAL. Le contexte physique PAL, l’effet du store, le
consumer XMA et tout packet audio restent inconnus.

## Garde et prochain test

La garde existante doit conserver l’expérience XMA hors production et l’ordinal
548 tant qu’une trace PAL indépendante ne joint pas `P/G/I/A/V` à un consumer
ou à un effet observable. Le prochain test utile est une sonde read-only du
consumer immédiat de la plage écrite, avec PC/LR/thread/tick et comparaison
neutral/START; aucun fallback Xenia ne doit être ajouté.

Capsule durable : `analysis/demo/ac6-demo-xma-xenia-generic-crossmatch-v1.json`.

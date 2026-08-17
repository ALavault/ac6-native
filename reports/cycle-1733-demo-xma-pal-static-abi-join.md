# Cycle 1733 — jointure statique PAL de l’ABI XMA

## Verdict

La tranche PAL autour de `0x82357240` est maintenant jointe aux bytes du
basefile et au code généré strict utilisé pour la démo. La fonction parcourt
une table d’entrées de stride `96`, appelle `XMACreateContext` avec `r3` égal à
`entry+64`, puis lit le résultat, appelle `MmGetPhysicalAddress`, écrit
`entry+80` et forme un store `stwbrx/eieio` non nommé. Le codegen PAL qualifié
ne contient que les imports XMA `XMACreateContext` (548) et
`XMAReleaseContext` (550) ; aucun appel direct `XMAInitializeContext` ou
`XMAEnableContext` n’est présent dans cette table d’imports.

Cela ne donne pas le droit de remplacer l’import par l’implémentation Xenia :
le retour, l’allocation du contexte et l’effet des stores MMIO restent
inconnus. Le chemin de production conserve donc le trap ordinal 548.

## Preuves PAL

| élément | preuve |
|---|---|
| cible | `Default.xex`, PAL, Xenon BE/Xenos |
| XEX / basefile | `de917873…5da8` / `b98a9ac1…4218` |
| fonction | `0x82357240`, `0xCC` octets, bytes `7436f840…ed9` |
| callsite | `0x82357294`, bytes `4801F5217C771B78`, retour `0x82357298` |
| table | count `+0`, flags `+4`, base `+8`, stride `96`, output slot `+64`, index `+80` |
| arguments observés | `r3=entry+64`, `r4=0`, `r5=0x6180`, `r6=0`, `r7=1` |
| imports XMA | `0x823767B4` create/548, `0x823767A4` release/550 |
| stores suivants | `0x823572D8: 7D60552C`, puis `0x823572DC: 7C0006AC` |

Les fonctions connexes `0x82357310`, `0x82357390`, `0x82357458` et
`0x823575A8` ont été vérifiées par leurs hashes de bytes/pseudocode dans la
capsule. Elles manipulent la table et des champs guest bornés, mais ne
fournissent pas de nom de registre XMA.

## Jointure runtime

Les probes neutral et START fraîches atteignent toutes deux, au tick 1048,
le thread 21 et l’import ordinal 548 avec `r3=0x17360050`, après 911
présentations. L’expérience opt-in a observé un store logique `1` vers
`0x7FEA1A80` après `MmGetPhysicalAddress`; cette valeur et cette adresse sont
`demo-observed` sous expérimentation, pas un effet matériel qualifié.

## Cross-match Xenia

Xenia `95a5c3ee…3eec`, fichier `xboxkrnl_audio_xma.cc`, confirme seulement la
forme générique d’un create à pointeur de sortie et documente les exports
548–551, un contexte de 64 octets et 320 entrées. Les bases génériques
`0x1A80`, `0x1940` et `0x1A40` ne sont pas copiées dans la démo PAL. Xenia n’a
pas été exécuté ni modifié.

## Classification et garde

- **demo-qualified** : identité PAL, bornes et hashes statiques, ABI d’appel,
  table, imports et égalité neutral/START de la frontière.
- **demo-observed** : progression jusqu’au trap et store MMIO opt-in.
- **xenia-generic** : noms/ordinals et forme générique uniquement.
- **unknown** : allocation/retour réel, registre MMIO, packets/timestamps,
  PCM, premier writer EDRAM non nul, pixels et mission.

Aucune mutation du runtime de production, de Ghidra, de Xenia/ReXGlue, du
code généré ou des microcodes n’a été faite. Aucun décodage audio, readback ou
screencap n’est promu.

Capsule : `analysis/demo/ac6-demo-xma-pal-static-abi-join-v1.json`.

## Prochain checkpoint

Instrumenter, sur deux stores frais neutral/START, le premier writer et le
premier consumer PAL après le create avec PC/LR/thread/tick. Tant que cette
preuve n’existe pas, conserver le trap ordinal 548 et rechercher la première
source EDRAM non nulle sans fallback.

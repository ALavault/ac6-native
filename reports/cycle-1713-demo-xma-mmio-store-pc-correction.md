# Cycle 1713 — correction de provenance du store MMIO XMA

## Verdict

Le frontier cycle 1712 reste valide, mais son adresse d'instruction devait
être séparée du LR fourni par le diagnostic runtime. Le trap expose
`LR=0x823572AC`, qui est le retour du `bl MmGetPhysicalAddress`. Le basefile
PAL qualifié place l'instruction `stwbrx` à `0x823572D8`; `0x823572AC` est un
`lwz` post-retour. Cette correction ne donne toujours aucune sémantique au
registre XMA `0x0C78` et ne modifie pas la route fail-closed.

## Identité et preuves

| élément | valeur |
|---|---|
| cible | `Default.xex`, démo PAL, Xenon big-endian/Xenos |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| basefile SHA-256 | `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| basefile longueur | `10420224` octets |
| fenêtre PAL | `[0x823572AC,0x823572E0)`, 52 octets |
| fenêtre SHA-256 | `5211109b1ef5928b6c967f88c116a07cc2fc3abb0f1db1288c787bd2e99c53a4` |
| source générée consultée | `.build/ac6-demo-atomic-runtime-1/codegen/generated/ppc_recomp.48.cpp` |
| source générée SHA-256 | `df90d3db8752bd32f26e0060a8d2e9c58dd38ccb4c212cddc71b87078454eaca` |

## Jointure instruction/LR

| adresse guest | bytes PAL | décodage | preuve |
|---|---|---|---|
| `0x823572A8` | `48 01 ED 7D` | `bl 0x82376024` (`MmGetPhysicalAddress`) | basefile |
| `0x823572AC` | `81 7C A5 2C` | `lwz r11,-23252(r28)` | basefile; LR runtime après le `bl` |
| `0x823572D8` | `7D 60 55 2C` | `stwbrx r11,0,r10` | basefile + codegen littéral |
| `0x823572DC` | `7C 00 06 AC` | `eieio` | basefile |

Le stderr des deux runs cycle 1712 rapporte `thread=21`, `tick=1048`,
`address=0x7FEA31E0`, `lr=0x823572ac`, avec `r11=1` et `r10=0x7FEA31E0`.
Le code généré passe donc l'argument bswap `0x01000000` à `PPC_MM_STORE_U32`;
c'est une preuve de l'argument du recompileur, pas une observation d'un effet
matériel. Aucun write effectif n'est promu puisque la plage MMIO est non
mappée et le trap survient avant effet.

## Classification

- **demo-qualified** : bytes PAL de l'appel et du store, adresse
  `0x7FEA31E0`, registre dword `0x0C78`, tick/thread/LR du trap, égalité
  neutral/START du cycle 1712.
- **demo-observed** : registre calculé et tentative de store dans
  l'expérience opt-in.
- **xenia-generic** : table ReXGlue/Xenia et son commentaire de plage
  inconnue; aucune transplantation.
- **unknown** : sémantique de `0x0C78`, endian matériel effectif, bitmap et
  contexte global, premier packet XMA, consumer PCM et audio anglais/japonais.

## Garde et prochain test

`XMACreateContext` reste un trap dans la route par défaut; l'expérience n'est
ni `play` ni `replay`. Ne pas mapper `0x0C78` depuis la table générique.
Le prochain test doit joindre une preuve PAL indépendante du registre (lecture,
écriture et effet ou second store) avec PC/LR/tick/thread; sinon revenir au
trap ordinal 548. Aucun décodage `vgmstream`/FFmpeg, readback ou screencap
n'est autorisé par ce reçu.

Capsule : `analysis/demo/ac6-demo-xma-mmio-store-pc-correction-v1.json`.

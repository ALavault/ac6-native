# Cycle 1716 — jointure arithmétique PAL de l'adresse XMA

## Verdict

La tranche statique PAL explique la divergence entre les deux adresses
observées sans donner de nom au registre XMA. `FUN_82356510` lit
`0x7FEA1800` avec `lwbrx` et écrit la valeur dans le global
`0x829DA52C`. `Function_82357240` soustrait ensuite ce global au pointeur
retourné par `MmGetPhysicalAddress`, quantifie le résultat par paquets de
64 octets et forme l'adresse du store par rotations PPC.

Les bytes montrent donc pourquoi une table expérimentale dont le global reste
nul produit `0x7FEA31E0` (`0x0C78`). Cela ne qualifie ni `0x0600`, ni
`0x0C78`, ni l'effet matériel. Aucun mapping supplémentaire n'est installé.

## Identité et sources

| élément | valeur |
|---|---|
| cible | `Default.xex`, démo PAL, Xenon big-endian/Xenos |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| basefile SHA-256 | `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| projet Ghidra | `ghidra-projects/ace-combat-6-demo`, `PowerPC:BE:64:Xenon` |
| `FUN_82356510` | taille `0x18`, bytes SHA `83dbe228ed39602a849d9a7655e1b117e9118aacd9b084058c93cb923a22d6d2` |
| `Function_82357240` | taille `0xCC`, bytes SHA `7436f8404267283916f2e64fdcda534788553fbf366daa330bc09fe9220ed9` |
| source statique | `analysis/demo/ac6-demo-static-semantics-v1.json` |
| contrôle littéral | `.build/ac6-demo-atomic-runtime-1/codegen/generated/ppc_recomp.48.cpp` |

## Forme exacte observée

En notant `P` la valeur retournée par `MmGetPhysicalAddress` et `G` le
contenu PAL de `0x829DA52C`, les instructions de `0x82357240` réalisent :

```text
I = ((P - G) >> 6) & 0xFFFF
A = 0x7FEA1A80 + ((I >> 5) << 2)
V = 1 << (I & 0x1F)
stwbrx V, 0, A
eieio
```

La constante `0x7FEA1A80` provient de `lis r29,8186; ori r29,34464`, puis
du `rlwinm` d'adresse. La preuve est arithmétique et binaire; aucun nom
Xenia/ReXGlue n'est transplanté.

| état du global | `P` | `I` | adresse formée | observation |
|---|---:|---:|---:|---|
| `G=P` | `0x2EEEC000` | `0x0000` | `0x7FEA1A80` | adresse expérimentale attendue |
| `G=0` | `0x2EEEC000` | `0xBB00` | `0x7FEA31E0` | trap cycle 1715, `0x0C78` |
| `G=0x2E800000` | `0x2EEEC000` | `0xBB00` | `0x7FEA31E0` | même résultat modulo `0x10000` |

La lecture de `0x7FEA1800` dans `FUN_82356510` précède l'écriture du global;
une valeur de table retournée par l'expérience ne suffit donc pas à prouver
quelle valeur de `G` est effectivement consommée. Le pointeur expérimental
`0x2E800000` donne le même `I` bas 16 que `G=0` dans cette soustraction.

## Classification

- **demo-qualified** : identité, plages/fonctions Ghidra, bytes/hash, formule
  PPC et adresses calculées pour les deux états de `G`.
- **demo-observed** : cycle 1715, contexte `0x2EEEC000`, trap
  `0x7FEA31E0`, thread 21/tick 1048/LR `0x823572AC`.
- **xenia-generic** : aucune sémantique nouvelle; le registre reste non nommé.
- **unknown** : valeur matérielle de `0x7FEA1800`, initialisation effective de
  `G`, effet de `A/V`, packets XMA, PCM et audio.

## Garde et prochain test

Ne pas mapper `0x7FEA1A80` ni `0x7FEA31E0` comme registres XMA. Le test
minimal suivant est read-only : sur deux traces fraîches neutral/START,
capturer la valeur de la lecture `0x7FEA1800`, le store PAL de `G` et les
arguments `P/G/A/V` juste avant `0x823572D8`, avec PC/LR/thread/tick. Toute
valeur non jointe conserve le trap ordinal 548 et l'expérience hors
production.

Capsule : `analysis/demo/ac6-demo-xma-address-join-v1.json`.

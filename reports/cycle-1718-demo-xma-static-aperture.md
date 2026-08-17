# Cycle 1718 — jointure statique des écritures XMA tardives

## Verdict

Les bytes PAL démo confirment quatre accès bruts supplémentaires dans la
tranche XMA, sans permettre de leur attribuer un nom de registre ou un effet
matériel. La valeur `0x7FEA1AC0` mentionnée comme candidat dans des notes
antérieures n'est pas produite par `FUN_82357310` pour l'index zéro : les
immédiats PAL donnent une base `0x7FEA1A40`. Cette correction est statique et
ne change aucune route runtime.

L'import `XMACreateContext` ordinal 548 reste fail-closed. Aucun mapping MMIO,
aucun décodage audio, readback ou screencap n'est ajouté.

## Identité et sources

| élément | valeur |
|---|---|
| cible | `Default.xex`, démo PAL, Xenon big-endian/Xenos |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| basefile | `.build/ac6-demo-atomic-runtime-1/codegen/xex-basefile.bin` |
| basefile SHA-256 | `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| atlas | `analysis/demo/ac6-demo-static-semantics-v1.json` (SHA `e723930a01e9ba573d0d899e16ebbd51f31c6c4553f53cf5ccafee5f50ba7962`) |
| canonical static atlas | `analysis/demo/ac6-demo-static-decomp-atlas-v1.json` (SHA `7ee1e677dfac287fdcd8d80b1c5f34575cbabf1c41ab79e70bd1581f87114e2d`) |
| cross-match littéral | `.build/ac6-demo-atomic-runtime-1/codegen/generated/ppc_recomp.48.cpp` |
| SHA cross-match | `df90d3db8752bd32f26e0060a8d2e9c58dd38ccb4c212cddc71b87078454eaca` |

Le fichier généré sert uniquement à relier les instructions aux adresses
guest déjà qualifiées par le basefile et l'atlas; il n'est ni modifié ni
utilisé comme source de sémantique.

## Accès démontrés par les bytes PAL

| fonction / PC | bytes | instruction | calcul ou valeur | statut |
|---|---|---|---|---|
| `0x82357310` / `0x82357360` | `7CA05D2C` | `stwbrx r5,0,r11` | pour `n=u16be(entry+80)`, `A=0x7FEA1A40+((n>>5)<<2)`, valeur logique `1<<(n&31)`, valeur wire `bswap32(V)` | `demo-qualified` structurel |
| `0x82357310` / `0x82357364` | `7C0006AC` | `eieio` | barrière immédiatement après le store | `demo-qualified` structurel |
| `0x82357390` / `0x82357408` | `7FC0512E` | `stwx r30,0,r10` | `r10=0`, base `r11=0x7FEA1804`, donc écriture brute `0x00000000` | `demo-qualified` structurel |
| `0x82357390` / `0x8235740C` | `7C0006AC` | `eieio` | barrière | `demo-qualified` structurel |
| `0x82357390` / `0x82357414` | `7D40592E` | `stwx r10,0,r11` | `r10=0x03000000`, même adresse brute `0x7FEA1804` | `demo-qualified` structurel |
| `0x82357390` / `0x82357418` | `7C0006AC` | `eieio` | barrière | `demo-qualified` structurel |
| `0x82357458` / `0x8235748C` | `7D605C2C` | `lwbrx r11,0,r11` | lecture brute à `0x7FEA1818`, suivie d'un `xori` statique; aucune valeur runtime jointe | `demo-qualified` structurel |
| `0x823575A8` / `0x823576C0` | `7D603D2C` | `stwbrx r11,0,r7` | base d'index zéro `0x7FEA1940+((n>>5)<<2)`, valeur logique `1<<(n&31)`, wire `bswap32(V)` | `demo-qualified` structurel |
| `0x823575A8` / `0x823576C4` | `7C0006AC` | `eieio` | barrière immédiatement après le store | `demo-qualified` structurel |

Pour `FUN_82357310`, le base `0x7FEA1A40` provient de
`lis r11,8186; ori r7,r11,34448`, soit `0x1FFA8690`, puis du décalage gauche
de deux bits. Pour `FUN_823575A8`, `lis r11,8186; ori r6,r11,34384` donne
`0x1FFA8650`, donc `0x7FEA1940` après le même décalage. Les deux fonctions
ajoutent ensuite le quotient `n>>5` avant le décalage; elles parcourent des
entrées de stride 96 octets. Le masque de table `flags & 0x30000` dans
`FUN_82357310` intervient dans le pointeur d'entrée, pas dans la base brute
calculée ci-dessus.

## Frontières et hashes des fonctions

| entrée | taille | bytes SHA-256 | pseudocode SHA-256 | atlas |
|---|---:|---|---|---|
| `0x82357240` | `0xCC` | `7436f8404267283916f2e64fdcda534788553fbf366daa330bc09fe9220ed9` | `9d2db824bcf8391231e9b809524e7acec3079f2204cfd3c28dd3e8ceda002a13` | succès / unknown |
| `0x82357310` | `0x80` | `5768c76bea572f55a15b8496790f685505c781f2f2259ff1237978a268005c2f` | `496813462c14fc650dc32743d63723292d94c58c416f7051211dfb6b2a5d2dc5` | succès / unknown |
| `0x82357390` | `0xC8` | `d0d5f3c29766eddc606cbb2b4b1aac072dea242e7659bf7a68d2437ff0d5003d` | `2fad2149027c50ddc4de01dc4a0c3e8de39b7d3b4f30c7aa4e5c08db28c086ce` | succès / unknown |
| `0x82357458` | `0x150` | `fa99339cb706c69835fec1f54e6c8fe1f24715954fd70ccc22223a5dacd94e1b` | `7aa386de4b1a6b1309925c24ecedd4aebfcfbee8ab1fd78199264b0de2c64a7f` | succès / unknown |
| `0x823575A8` | `0x148` | `0170decfcb699ad75b759421f98de13d72ccd3359bc6f17cd7aa2e19e97a6c9a` | `88586ba8ccd84e4de689e66619aea4ecf94e5274b0b916425e3b557d5d665b61` | succès / unknown |

## Classification

- **demo-qualified** : identité PAL, frontières et hashes, instructions PAL,
  adresses brutes et arithmétique des bases d'index.
- **demo-observed** : aucune nouvelle observation runtime dans ce cycle; les
  observations neutral/START du cycle 1717 restent inchangées.
- **xenia-generic** : aucune preuve importée.
- **unknown** : effet des adresses, registre XMA correspondant, contenu des
  paquets, timestamps, volume, PCM et consumer audio.

## Garde et prochain test

La correction est documentaire : ne pas mapper `0x7FEA1A40`, `0x7FEA1940`,
`0x7FEA1804` ou `0x7FEA1818` dans le runtime sur cette seule base. Le prochain
test minimal est une sonde opt-in fraîche neutral/START qui capture les stores
à ces adresses avec PC/LR/thread/tick, en conservant la route par défaut et le
trap ordinal 548. Sans effet indépendant, aucun décodage XMA ou readback n'est
autorisé.

Validation du checkpoint : CTest codegen-ON `17/17` dans
`.build/ac6-demo-atomic-runtime-1` et codegen-OFF `18/18` dans le build
canonique `recompilation/ace-combat-6-demo/build`; aucune modification de C++
généré, Ghidra, Xenia/ReXGlue, microcode ou actif propriétaire.

Capsule : `analysis/demo/ac6-demo-xma-static-aperture-v1.json`.

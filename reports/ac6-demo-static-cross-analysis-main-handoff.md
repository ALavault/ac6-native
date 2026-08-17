# Handoff main thread — croisement statique AC6 PAL

Date : 2026-08-16  
Cible exclusive : démo PAL `Default.xex`  
SHA-256 : `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`

## Archive contrôlée

`ac6-static-cross-analysis-20260816.tar.zst` mesure 450 488 octets et porte le
SHA-256 `32d3c6fa1abd9dc9479e22f5f5681674c66b38e682525a70afff1f49859b378b`.
Les 54 entrées de son `SHA256SUMS` passent. L'extraction de contrôle est restée
sous `/fastdata/lavaulta/tmp` ; aucun actif de l'archive n'a été copié dans le
projet.

Les identités déclarées par l'archive correspondent aux fichiers locaux :

| Source | SHA-256 |
|---|---|
| `Default.xex` | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| `DATA.TBL` | `0d9e11cf19881971e7d14c0077e9e719c1795e0316afab4b48b153351591eef8` |
| `DATA00.PAC` | `838356ade0f41fc7eee11684dda8e4d6c07eac7512a23ef1d148eb3144dbb162` |
| `DATA01.PAC` | `08ef13fe61caf0b072a4de6de577e965b4f1c8feb88d638ffc099dd4d63238d3` |

Le XEX extrait est bien le module titre `4E4D87E6`, base `0x82000000`, entrée
`0x821A7160`, image `0x9F0000`, version `0.0.1.2`. Cette identité ne vaut que
pour la démo PAL ; aucune preuve retail n'est fusionnée.

## Croisement avec le gate shader local

Les quatre tables shader centrales de l'archive sont byte-identiques à celles
de `ac6-pal-shader-identification-20260816.tar.zst` déjà utilisées :

| Table | SHA-256 | Résultat |
|---|---|---|
| `nsxr-map-terrain-shaders.tsv` | `e4485251ca9f5cc3e989652178d4c67216de95b0f6b66ade9bfa89b8ee8ba0a9` | identique |
| `nsxr-shader-canonical.tsv` | `32da44938ad32847ab644a3d7532bd2882cd45ddd9e11d644a54d318f5a9082c` | identique |
| `basefile-pal-shader-matches.tsv` | `b1ec58bfc4c989abd383c5abc723d0a7dc750fb0340e945f8fc4235264bea5e3` | identique |
| `shader-source-attribution.tsv` | `f441d544992355b7e2b49870814ca6a8d39b400144cdcf19646fd76e6b94b736` | identique |

Les clés `(SHA canonique, NSXR, record_offset)` des 108 lignes Map/terrain sont
exactement égales aux 108 occurrences du gate local. Les 108 rejoignent aussi
une ligne de `shader-texture-slots.tsv`, SHA-256
`0c37972a87683be0744167190ba25cd69178f2b5512bdcd3691ed81205ea61d2`.

Le verdict local reste donc directement applicable au corpus de cette archive :

- 72/78 microcodes uniques qualifiés hors ligne ;
- 28/28 PS et 44/50 VS ;
- 10 occurrences d'alias vertex normalisées sous garde exacte ;
- 6 VS encore fail-closed, tous sur
  `VSPointSizeEdgeFlagKillVertex.x` (`vsPts_DIRECTPOS`, `vsMapStar`,
  `vsMapStarSat`, variantes Map et Map_HDR) ;
- reçu durable : `analysis/demo/ac6-demo-map-shader-offline-gate-v1.json`,
  SHA-256 `701f113efe8a22c1b0429ad1bc5aeda4d3bee71e09f60eb7155e0f7c8dbc26cd` ;
- reçu temporaire complet de deux runs frais :
  `1aa151beb1d43ac6b65c699c48d64e6a0397ac907d68eb5da51ae766a473297e`.

L'archive n'ajoute donc aucune nouvelle identité Map, mais fournit une table de
slots texture utile. Les signatures `tf0`, `tf0,tf1`, `tf10,tf11` ou slots
hauts réduisent les familles candidates ; elles ne sélectionnent pas une paire
VS/PS exacte pour un objet.

## Shader atteint `586168ec…a83cc0`

La présence basefile à `0x14140`, taille 60, et les hashes raw/swap32 concordent.
Une analyse locale fraîche de la plage PAL exacte par le CLI ReXGlue épinglé
produit :

```text
vfetch_full r0.xy11, r0.x, vf95,
  DataFormat=FMT_32_32_FLOAT, Stride=2
max oPos, r0, r0
```

Le shader est donc un VS minimal pass-through de position 2D : ce rôle peut être
promu de `unknown` à `demo-qualified`. Il n'appartient pas au corpus NSXR
Map/terrain. Le qualificatif « utilitaire/fullscreen » reste une attribution de
contexte de draw, pas une propriété portée par le microcode seul.

Divergence à conserver : `provenance/basefile-586168-analysis.txt` écrit `vf0`,
alors que ReXGlue décode `vf95` sur les mêmes 60 octets. Format, stride, sortie
et rôle pass-through concordent ; l'index de fetch ne doit pas être importé
depuis l'archive sans résoudre cette divergence. Le désassemblage local frais a
le SHA-256 `52e5e6b3441d956d5a5ee43c2fb9dc01638e160370b33510cab254fdcdb23328`.

## Jointure objet, matériau et texture

Les tables de l'archive établissent structurellement :

```text
NDXR -> NU_HASH / descripteur matériau -> GIDX -> NTXR
```

Contrôles directs des TSV :

- table conventionnelle : 904 lignes, 338 noms, 18 hashes matériau non vides
  et 175 GIDX distincts ;
- terrain `mapparts` : 170 lignes, 170 objets/hashes/GIDX distincts ;
- 169 relations utilisent un descripteur `0x30000010` ;
- une relation, `mapparts_m01_m_021_60_O_OBJ`, reste explicitement un
  `aligned_gidx_fallback` vers `0x000008F8`.

Ces relations sont des preuves statiques liées aux PAC démo, mais elles n'ont
pas été régénérées dans ce checkpoint. Avant intégration canonique, rejouer les
trois parseurs de l'archive sur les PAC qualifiés et exiger les mêmes TSV. La
relation fallback doit rester séparée des 169 preuves structurelles.

Surtout, aucune table ne prouve `objet -> shader exact`. L'archive l'indique
correctement : le consommateur de `NU_FLAG1/2`, de l'interface vertex et du
nombre de textures reste à suivre jusqu'au descripteur NSXR.

## PPC et décompilation : accords et rejets

L'atlas canonique local confirme des frontières et décompilations réussies aux
adresses citées par l'archive :

| Adresse | Hash bytes atlas | Lecture admissible |
|---|---|---|
| `0x822E8628` | `bbac3c73…b960b38c` | validateur magic `NDXR` |
| `0x822F3768` | `69b44bd4…f00c716` | validateur versions `NTXR` |
| `0x8227A898` | `bdbd5c84…85c5a435` | logique inflate/DEFLATE candidate |
| `0x8227A5E0` | `77a41866…02df57cf` | fonction établie, rôle `keygen` non établi |
| `0x821A3C30` | `bb41ae12…ec9cfb3a` | frontière établie, rôle `PAC loader` rejeté |

Deux noms de fichiers de l'archive ne doivent pas devenir des symboles Ghidra :

1. `ppc/pac-loader-821a3c30.asm` contredit la preuve dynamique PAL. Les runs
   neutral/START joignent `0x821A3C30`, tick 0/thread 1, au corridor du slot
   XMA/zero-fill et à l'appel fill de `0x823273E0` depuis `0x821A3E70`.
   `PAC loader` est donc rejeté comme sémantique.
2. `ppc/keygen-8227a5e0.asm` ne prouve pas que `0x8227A5E0` génère la clé XOR
   Blowfish/π. Sa forme est compatible avec la construction de tables inflate.
   Le codec PAC local reste prouvé par les 40 sorties exactes, mais la jointure
   à cette fonction guest demeure `unknown`.

`0x8227A898` renforce en revanche l'identification du flux codec 1 comme
DEFLATE : gestion des types de blocs et contrôle LEN/NLEN sont visibles. Cela
ne suffit pas à nommer la fonction appelante ou à joindre l'ordinal PAC sans
xref supplémentaire.

## Gameplay et audio

L'archive fournit des axes statiques utiles mais encore indépendants du gate
shader : 762 classes RTTI, une `CAce6MissionManagerCampaign` fondée sur
`CHsm<...,8>`, neuf pistes BGM, 738 entrées voix par langue, deux demopacks par
langue, et 560 tons nuSound2 physiques (455 XMA, 105 WAV). Les tables audio ont
passé les sommes internes, mais aucun flux n'a été redécodé ici avec
`vgmstream-cli`/FFmpeg.

Conserver comme `unknown` : noms/transitions des huit états, table radio
concrète, jointure événement logique -> ton physique, packet/timestamps XMA et
équivalence PCM bilingue. Ne pas relier l'adresse `0x7FEA1A80` à un pack depuis
ces seuls inventaires.

## Ordre d'intégration recommandé

1. Mettre à jour le reçu sémantique du shader `586168` avec le rôle
   pass-through et la divergence `vf0`/`vf95`.
2. Rejouer les parseurs NDXR/MATE/NTXR sur les PAC démo et publier un reçu
   metadata-only ; isoler le fallback terrain.
3. Depuis `0x822E8628`/`0x822F3768`, suivre le consommateur de `NU_FLAG1/2`,
   des attributs vertex et des slots jusqu'au choix exact VS/PS.
4. Garder `0x821A3C30` dans le corridor XMA observé ; ne pas importer le nom
   `PAC loader`. Garder `0x8227A5E0` sans rôle confirmé.
5. Fermer les six VS `PointSize` via un adaptateur test-only et un draw points
   borné avant d'étendre le gate aux 1 891 microcodes uniques.
6. À la première provenance XMA exacte, comparer `vgmstream-cli` et FFmpeg ;
   ne pas décoder préventivement tout l'inventaire audio.

## Politique

- aucune preuve retail fusionnée ;
- aucun projet Ghidra, checkout Xenia/ReXGlue/XenosRecomp, C++ généré ou
  microcode modifié ;
- aucun XEX, PAC, FHM, microcode, HLSL/SPIR-V généré ou audio propriétaire
  ajouté au projet ;
- toutes les attributions de l'archive qui dépassent les bytes ou les tables
  recoupées restent explicitement `archive-inference` ou `unknown`.

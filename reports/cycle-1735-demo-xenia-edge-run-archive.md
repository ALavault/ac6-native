# Cycle 1735 — archive d’un run Xenia Edge de la démo PAL

## Verdict

L’archive est valorisée en lecture seule. Elle contient bien le `Default.xex`
PAL ciblé et un run Vulkan Edge qui se termine proprement à la frame Xenia
4183. Elle fournit un corpus HIR et shaders utile pour le cross-match, mais
aucune trace GPU `.xtr` et aucune couverture dynamique de fonctions. Elle ne
ferme donc pas une lane native et ne justifie ni pixel, ni audio PCM, ni
progression de mission dans `ac6-demo-recomp`.

## Identité et intégrité

Archive : [`xenia-edge-ac6-decomp-20260816-104009.tar.zst`](../xenia-edge-ac6-decomp-20260816-104009.tar.zst)

| Élément | Valeur |
|---|---|
| SHA-256 archive | `098661057a6d6fc5147d7b0add31c24f2cbe8296b4e22438a293804bf28864d2` |
| run | `2026-08-16 10:40:09–10:41:58` (Europe/Paris) |
| paquet | `3377D814ABAE45D844E90826823AF6AB5D5F2F2B4E` |
| Title ID / Media ID | `4E4D87E6` / `565E01A0` |
| Xenia Edge | `15200f6447e6b1f3676e09f0e7c065b3bf57f92d` |
| GPU / pilote / backend | NVIDIA RTX 500 Ada Generation Laptop GPU / 595.84 / Vulkan |
| `Default.xex` | 1 454 080 octets, SHA `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |

`SHA256SUMS` couvre 13 558 fichiers et retourne `13558 OK, 0 FAILED, 0
missing`. Les sommes durables de contrôle sont : `MANIFEST.md`
`8e414764…49f713`, `SHA256SUMS` `7f919dc4…d4453`, configuration
`140180f6…1a1da`, lanceur `f9b3daec…5d71` et journal
`929151b7…cde1c`.

Le lanceur archivé laisse `storage_root`, `content_root`, `cache_root` et les
slots XUID vides. Pour éviter de recréer le profil, employer le lanceur
persistant du projet [`scripts/run_xenia_edge_native.sh`](../scripts/run_xenia_edge_native.sh),
qui fixe `.tools/xenia-edge-profile` (ou `XENIA_EDGE_PROFILE_ROOT`). Aucun
profil ni actif du bundle n’a été copié dans le projet.

## Corpus de décompilation fourni

- 12 922 fichiers HIR, adresses `0x82090000`–`0x823767B4` ; environ 636
  fonctions ont été découvertes ou réémises tardivement.
- 630 fichiers shader : 87 VS, 91 PS, 14 géométries internes, 178 microcodes
  binaires, 178 désassemblages texte et 260 variantes SPIR-V.
- Le fichier `dumps/gpu/` est vide. La build Release a
  `NDEBUG`/`XE_ENABLE_TRACE_WRITER_INSTRUMENTATION=0`, donc
  `trace_gpu_stream=true` ne produit pas de `.xtr`.
- Le log indique explicitement `Stack walker unimplemented on posix` puis
  `Disabling --debug due to lack of stack walker`. Les HIR ne constituent donc
  pas une couverture d’exécution.

HIR relus sur les adresses déjà jointes au PAL (bytes textuels produits depuis
le même XEX, statut `demo-observed` statique; leur IR reste `xenia-generic`) :

| Fonction | Preuve dans le HIR | SHA-256 du HIR |
|---|---|---|
| `0x821B0D20` | `0x821B0D70: 954B0004`, `stwu r10,4(r11)` | `d4136d84…88409` |
| `0x821B9BC8` | `0x821B9D24: 7D2AC12E`, `stwx r9,r10,r24` | `d4db00a3…c893c` |
| `0x821B9F70` | `0x821BA01C: 94CA0004`, `stwu r6,4(r10)` | `2afbf819…a8cfe` |
| `0x82357240` | appel nommé `XMACreateContext` à `0x82357294`, puis `MmGetPhysicalAddress` | `dc767fb8…e6093` |
| `0x821A7160` | point d’entrée de module déclaré par le manifeste | `9722245f…f3fb` |

Ces lignes recoupent les bytes PAL déjà obtenus par `rr`, mais le run Edge ne
fournit pas le PC dynamique qui aurait écrit l’IB `0x1274A000`; aucune
provenance runtime de cette plage n’est promue.

## Correspondances shader calculées

Le hash AC6 est calculé sur les dwords Xenon big-endian. Pour comparer les
fichiers Xenia `*.ucode.bin.*`, chaque mot de 32 bits a été inversé en mémoire
temporaire, puis SHA-256 recalculé. Cette opération est une valeur calculée,
pas une sémantique importée de Xenia.

| Hash microcode AC6 | Rôle observé côté démo native | Fichier Edge | Taille | SHA fichier Edge | SHA après swap32 | Statut |
|---|---|---|---:|---|---|---|
| `099625f3…e4e3` | VS atteint | `shader_C049A8C9E556F129.ucode.bin.vert` | 96 B | `fcb772a9…a390c` | `099625f3…e4e3` | correspondance exacte calculée |
| `93488cb9…402b` | VS atteint | `shader_0A6D1DD7767FDF27.ucode.bin.vert` | 108 B | `6e8911a2…b8a25` | `93488cb9…402b` | correspondance exacte calculée |
| `4913603d…8e25` | PS atteint | `shader_2E372EA28CC404B7.ucode.bin.frag` | 36 B | `f9b2db9e…d0d79c` | `4913603d…8e25` | correspondance exacte calculée |
| `586168ec…3cc0` | VS de la route resolve/present | aucun | — | — | — | absent des 178 binaires, inconnu |

Le log associe les trois premiers identifiants Xenia à des traductions
SPIR-V/pipelines (par exemple `C049…` avec `2E372…`, et `0A6D…` avec
`2E372…`). Il ne contient aucune occurrence de `586168EC…`; aucun container
démo n’est inventé.

## Ce que le run Edge montre du renderer

Faits `xenia-generic` observés dans le log, utiles comme points de comparaison
mais non comme preuve du chemin natif :

- swapchain Vulkan créée en 1280×720, format hôte 44, espace couleur 0
  (`log`, ligne 623) ;
- au frame 0, Xenia crée un RT couleur 640×1024 4×MSAA (format invité 0,
  base EDRAM 0) et un depth 640×1024 (format 1, base 720) ;
- aux frames 0/1, une texture tiled 1280×720 `k_8_8_8_8`, pitch 1280,
  taille `0x00398000`, est chargée à `0x1A9A0000` ;
- au frame 4183, le log charge entre autres des textures tiled 1280×720
  `k_8_8_8_8`, 640×360 `k_16_16_FLOAT`, 640×360 `k_32_FLOAT` et
  320×360 `k_8_8_8_8`.

Le log ne contient qu’une occurrence textuelle de `VdSwap` (la table des
imports), pas un appel dynamique décodable. Les adresses Edge
`0x1A9A0000`, `0x1B800000`, etc. ne doivent pas être substituées à la
destination native qualifiée `0x1374A000`.

## Audio et terminaison

Le journal contient 180 lignes `XmaContext … Work loop exit`; toutes ont
`error_status=0`. Les identifiants de contexte observés vont de 0 à 31.
Cela prouve l’absence d’erreur Xenia dans ce run, pas le décodage ou la
sélection d’un flux XMA PAL. Aucun paquet brut, timestamp, volume, PCM ou
comparaison FFmpeg/vgmstream n’est dans l’archive.

La dernière ligne est `Cheap-skate exit!` au frame 4183. Aucun `device lost` ou
crash Vulkan n’est signalé. Cela reste une observation de l’oracle Edge ; le
frontend, les pixels et le résultat de mission natifs restent inconnus.

## Classification et prochain checkpoint

| Classe | Éléments |
|---|---|
| `demo-observed` | identité XEX exacte, métadonnées du run, bytes PPC textuels des HIR, lignes de fin et compteurs du log |
| `xenia-generic` | HIR/IR Xenia, identifiants shader 64 bits, SPIR-V/pipelines, EDRAM/texture log et comportements POSIX de la build |
| `demo-qualified` | aucun nouveau résultat natif; les IB/frontbuffer déjà qualifiés restent ceux des rapports PM4/Vulkan antérieurs |
| `unknown` | couverture dynamique des HIR, producteur Edge de `0x1274A000`, correspondance `586168…`, contenu EDRAM/pixels, resolve natif, PCM/XMA et mission |

Checkpoint recommandé : conserver les trois correspondances shader comme
cross-match hors ligne, puis obtenir une build Edge Debug/Checked avec writer
GPU actif pour produire un `.xtr` borné au premier frame. Comparer alors les
packets/IB au corpus démo, sans importer de noms, de microcode ou de sortie
générée dans le runtime. En parallèle, poursuivre côté natif le premier
writer EDRAM non nul et le premier consumer XMA; aucun fallback visuel/audio
n’est autorisé.

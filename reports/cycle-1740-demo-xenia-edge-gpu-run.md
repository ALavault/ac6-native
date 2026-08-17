# Cycle 1740 — dump GPU Xenia Edge de la démo PAL

## Verdict

L’archive est maintenant valorisée en lecture seule. Contrairement au run Edge
Release précédent, elle contient 23 traces XTR structurées, des snapshots EDRAM
bruts et une capture PNG par trace. Les trois traces gameplay inspectées ont
chacune un snapshot EDRAM de 10 MiB non nul et un événement swap.

Cela reste une observation de l’oracle Xenia Edge : les XTR ne joignent pas les
IB PAL qualifiés (`0x127CA0C0/11`, `0x1274A000/3029`), et l’archive ne contient
pas le XEX. Aucun pixel, resolve ou writer EDRAM n’est donc promu dans
`ac6-demo-recomp`, et aucune source synthétique n’est remplacée.

## Identité et intégrité

| Élément | Valeur |
|---|---|
| Archive | `xenia-edge-ac6-gpu-run-20260816-113443.tar.zst` |
| SHA-256 archive | `ef420b2ed4c8f71229e937a2ad695afcbfa82e0216e51b3ef1f6b911ace6d7e8` |
| Manifest SHA-256 | `131ee15b6134b545f3555feded20d8a853531a6f636a2814a9201a68bda730a4` |
| `SHA256SUMS` SHA-256 / entrées | `0bb08316831fc6dd6078421cbcd6b2fb1e88525e616a0f63ad6d82c394681d48` / 684 |
| Target déclaré | `4E4D87E6`, `Default.xex`, PAL, SHA attendu `de917873…5da8` |
| Xenia Edge | commit `15200f6447e6b1f3676e09f0e7c065b3bf57f92d`, Vulkan |
| GPU | NVIDIA RTX 500 Ada Generation Laptop GPU |

Le XEX n’est pas membre de l’archive : l’identité PAL est donc une déclaration
du manifeste et des en-têtes XTR (Title ID), pas une nouvelle vérification du
fichier binaire.

## Traces gameplay

Les XTR ont été lus avec le protocole local ReXGlue/Xenia
`include/rex/graphics/trace_protocol.h` : en-tête v1, commandes primary/IB,
packets, lectures/écritures mémoire, `EdramSnapshot`, registres, gamma et
événement swap. Les comptes et hashes suivants sont ceux des membres livrés et
de leurs snapshots bruts ; aucun payload n’a été ajouté au projet.

| Trace | XTR SHA-256 | packets | IB start/end | snapshot EDRAM | octets non nuls | swap |
|---|---|---:|---:|---|---:|---:|
| `5152.xtr` | `31a53c20…9c8b3ea` | 36 185 | 18/18 | `b78bb470…0ae70a8b3`, 10 485 760 B | 5 860 457 | 1 |
| `5505.xtr` | `2462c255…fb257678` | 36 644 | 18/18 | `f8de6878…c6863e3`, 10 485 760 B | 5 126 138 | 1 |
| `5849.xtr` | `16a06077…ec49502` | 36 248 | 18/18 | `da37a4ac…228790e`, 10 485 760 B | 5 286 025 | 1 |

Les PNG associées sont respectivement `626c5914…c29a39e5`,
`a9e22d74…6ddb9c26d` et `94655bd6…661e6c50`. Elles montrent le gameplay de
l’oracle Edge, mais ne sont pas des screencaps guest-owned du renderer natif.

## Jointure avec la démo PAL native

Les références PAL sont l’IB intermédiaire `0x127CA0C0`, 11 dwords,
`ef7ab6e4…d2b0`, et l’IB principal `0x1274A000`, 3029 dwords,
`d121c8d8…358d6`. Dans les trois XTR inspectés, aucune lecture mémoire ne
couvre ces plages et aucun IB fermé ne porte ces bases. Les bases Edge
`0x199…`/`0x1A…` restent propres à l’oracle. Le log Edge
(`efa6e0af…b077b2`) rapporte 24 créations RT/EDRAM, 364 `Draw skipped`, aucun
`VdSwap` dynamique et aucun resolve GPU explicite.

Conclusion : le snapshot EDRAM non nul est `xenia-generic`/`demo-observed`, pas
`demo-qualified`. Il ne donne ni le PC/LR guest du writer PAL, ni le contenu de
la surface source `0x1374A000`, ni une parité pixel avec Vulkan natif.

## Microcodes

Le corpus contient des correspondances calculées après inversion de chaque mot
32 bits : `099625f3…e4e3` → `shader_C049A8C9E556F129.ucode.bin.vert` (96 B),
`934488cb…402b` → `shader_0A6D1DD7767FDF27.ucode.bin.vert` (108 B) et
`4913603d…8e25` → `shader_2E372EA28CC404B7.ucode.bin.frag` (36 B).
`586168ec…3cc0` reste absent. Ce sont des cross-matchs de bytes, pas des
containers ou rôles AC6 importables ; le manifeste précise aussi qu’une partie
du corpus provient du cycle précédent et n’est pas forcément atteinte dans ces
traces.

## Classification et suite

| Classe | Résultat |
|---|---|
| `xenia-generic` | protocole XTR, paquets/IB Edge, snapshots EDRAM, événement swap, journal Vulkan |
| `demo-observed` | Title ID/identité déclarée, en-têtes XTR et couples XTR/PNG du run |
| `demo-qualified` | aucun nouveau élément ; les IB/frontbuffer PAL antérieurs restent l’autorité native |
| `unknown` | jointure IB, writer EDRAM PAL, resolve/pixels natifs, XMA PCM, mission |

Checkpoint recommandé : capturer dans le runtime PAL le premier résultat non nul
du draw rectangle avant `RB_COPY`, avec PC/LR guest, thread, tick, état RT/depth,
fetch/constants et hash de plage. Le dump Edge sert à comparer la structure et
les familles de microcodes, jamais à injecter pixels, shaders ou EDRAM dans
`play`/`replay`.

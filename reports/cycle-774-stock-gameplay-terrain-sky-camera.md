# Cycle 774 — frame gameplay, terrain, sky/cloud et caméra

Date : 2026-08-04

## Résultat

Une frame Vulkan native de Mission 01 est inventoriée après le HUD et après
les entrées de vol `w`, `d`, `q` et throttle. Ce n'est ni une caméra CUT, ni le
`fit_mesh` Mission 2. Le timing guest est stock : `ac6_unlock_fps=false` est
relu dans le runtime et les trois hooks timing diagnostiqués restent inactifs.
Le run appartient toutefois à la lane `bridge`, déclarée au boot avec
`save-dialog-synthesis,force-cvars,fallback-allocator` : il qualifie les joins
graphiques et l'exécution observée, mais ne prouve pas le comportement stock de
la campagne, du gameplay ou du scénario.

La frame GPU `11868` contient 1 340 draws, 2 428 vertex fetches et 4 094 lignes
de binding texture (2 047 bindings uniques). Le join content-addressable relie
directement :

- le terrain Mission 01 à l'entry 119, avec 408 occurrences de ranges NDXR ;
- le paquet sky/cloud gameplay à `entry119/022_FHM`, avec ses huit NDXR et ses
  NTXR réellement liés à des images/views Vulkan non nulles ;
- les matrices de caméra de vol c218–c221 aux draws sky et terrain de cette
  même frame.

L'avion joueur n'est pas revendiqué comme fermé. L'entry 9 identifie bien
`o_f16c_lod1` à `lod4` (`root.1.m16.10` à `.13`) et le modèle détaillé
`r_f16c_dd` (`root.1.m76.6`), mais aucun vertex hash exact de ces ranges ne
joint la frame 11868. Une copie vertex animée ou l'absence du modèle dans la
vue courante restent possibles ; il faudra le hash direct de l'index buffer ou
un autre raccord runtime, pas une pose/rotation inventée.

## Qualification gameplay et caméra

Le draw `mapobj_m01_l_brg2_n` arme la capture à 10:23:23, frame 10068. Les
captures synchronisées atteignent ensuite le HUD à 10:23:33, puis les entrées
pitch, roll, yaw et throttle jusqu'à 10:23:50. La frame cible 11868 est exécutée
à 10:25:36 et clôturée à 10:25:37 avec 1 340 draws. Elle est donc postérieure
de plus de deux minutes au trigger et de plus de 105 secondes à la dernière
entrée de vol.

Le terrain (exemple draw 259) consomme :

```text
c218 4042A439 BE945706 378B90D7 BECC2A31
c219 321F6279 40BB84F5 36AAFE85 BDFA23DF
c220 BFAACAB8 BF290DE9 381F0E12 BF68ACB9
c221 449E4EBD C589D5D2 3F79054E 44239AE7
```

Le sky (exemple draw 27) utilise la même orientation de caméra avec une
translation sky propre. Les captures HUD/radar changent après les commandes,
alors que le monde présenté reste noir. Cette dernière observation qualifie la
caméra et la route gameplay, pas la visibilité du monde.

## Terrain et sky/cloud exacts

Exemples de vertex ranges entry 119 consommés en frame 11868 :

| paquet | nom retail | draw | bytes | XXH3 |
|---|---|---:|---:|---|
| `021/.../010_NDXR` | `mapparts_m01_l_034_separate_0_6_10` | 259 | 41 600 | `7209F3DEB7BD097D` |
| `021/.../059_NDXR` | `mapparts_m01_m_021_59` | 328 | 4 736 | `13ADC9C725CD9932` |
| `021/.../063_NDXR` | `mapparts_m01_m_029_63` | 335 | 16 544 | `6BE28392DD9AE4B7` |
| `021/.../144_NDXR` | `mapparts_m01_m_044_144` | 394 | 5 376 | `8CAAC46D034E93A0` |
| `021/.../163_NDXR` | `mapparts_m01_l_027_02_separate_0_3_163` | 416 | 198 976 | `DABDB7B7C72E335F` |
| `021/.../169_NDXR` | `mapparts_m01_l_033_06_169` | 442 | 27 584 | `E1EBA6D3D5C26AF9` |

`entry119/022_FHM/000` porte les paramètres `.sky1.*` et
`.B.FlatCloudAlpha`. Les huit NDXR de `022_FHM/005_FHM` joignent les draws
27, 34, 41, 48, 55, 62, 69 et 76 avec VS `D6672F96583E649E` et PS
`3EF97C8B113CA332`. Le join texture relie 883 bindings uniques aux allocations
NTXR exactes de l'entry 119 ; par exemple le draw 27 lit GIDX `10001C56`,
256x256 format guest 20, image/view hôte non nulles. Il s'agit du paquet
Mission 01 réellement exécuté, pas d'un nom cloud de CUT.

## Contrôle mapobj et avion blanc

Sous ce timing stock, les joints du cycle 760 se reproduisent :

- `brg2_n` : frame 10068, draw 468, vfetch `045E8C40+34240`, texture
  `045FB000`, image/view non nulles ;
- `brg1_n` : frame 10069, draw 514, vfetch `04597990+13952`, texture
  `045A2000`, image/view non nulles.

Le contrôle hangar frame 5287/draw 51 lie deux textures valides et visibles.
Le draw sélectionné de l'avion blanc frame 9859/draw 138 utilise un fetch type
2 valide, BC3 guest format 20, 256x256 tiled, base `06B30000+65536`, mip
`06B40000+32768`, source lisible XXH3 `6C1BE48B64630BA2`, format Vulkan 137,
image/view/descriptor non nuls et sampler non nul. La classe A
« invalid/null fetch » est donc exclue ; B, C, D et E restent ouvertes. Le flag
global unsafe `gpu_allow_invalid_fetch_constants` n'a pas été activé.

## Sortie présentée et validations

Le HUD vert et le radar restent visibles, mais la zone monde est noire au
baseline et après pitch. Les pixels ensuite détectés dans la ROI monde suivent
le déplacement du HUD pendant roll/yaw ; ils ne sont pas revendiqués comme
`flight_world_pixels`. Le premier étage noir du render graph reste à localiser.

- build runtime `ac6recomp` : succès ;
- run Xvfb privé `:97`, 360 s, sortie normale du harness : succès ;
- analyse syntaxique et join content-addressable : succès ;
- build reconstruction RelWithDebInfo : succès ;
- CTest PAL : 63/63, skip SDL dummy attendu ;
- aucun Xenia lancé ; aucun processus étranger touché.

Le cycle 773 est un non-résultat de harness : le calendrier de captures par
défaut a retardé le premier `wait` jusqu'au timeout. Le cycle 774 corrige
uniquement ce paramètre par `--capture-at 0` et utilise un profil neuf.

## Identité du run

- workspace : `442c6dbcd5188fb84b056293a3ce7a000bd20669` ;
- diff suivi workspace : `2da4ac71c1f8ed4cf56e825b5679bd2ef874e1f7008edf4de2e4a78653188e43` ;
- runtime/RexGlue vendored : `b8b03c7a89dc7f23bcd7844d15aa5080d480bf11` ;
- diff suivi runtime : `b0bf93c5434bde7ad8058ee203c07864ec080447d74f722b90db6ada58ecd1d9` ;
- exécutable : `6853ae97d83e1b0c5a167ba65a702354ca0d73caab974c1c980b9962951a5877` ;
- TOML : `fed716e3ff77b50e4866e2a67c5a183f21651f6cf29fdae930091c3fdf1c85b0` ;
- XEX : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- DATA.TBL : `82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5` ;
- DATA00.PAC : `c3ed20ec6ef0260671d9cd5f3e088fab2a8d983cb6739efab350c87c6fb74816` ;
- DATA01.PAC : `eddb687418d4b49e36dd8b4e06f387e79be9c0792e97ea3405ab00dab76c03b4`.

Configuration effective : backend Vulkan RexGlue, chemin RT par défaut/FBO
(`render_path=0`), 1280x720, échelles 1x, `direct_host_resolve=false`, MSAA 2x
natif actif, `ac6_fix_deswizzle=true`, `SDL_AUDIODRIVER=dummy`, display `:97`,
profil utilisateur isolé. GPU NVIDIA RTX PRO 4000 Blackwell, pilote 595.84,
Vulkan 1.4.329.

Le dummy audio isole un défaut d'initialisation SDL vis-à-vis de l'état
PipeWire/WirePlumber partagé ; il ne vaut pas validation audio 1:1. L'instance
Ollama partagée et les processus Pharaoh/AC5 étrangers ont été inventoriés et
laissés intacts.

## Prochaine frontière

1. Ajouter au même inventaire borné le hash de l'index buffer et joindre le
   F-16 joueur à un LOD exact, sans rotation ni changement de caméra forcé.
2. Construire le flux ordonné clear/draw/RT/resolve/swap de la première frame
   noire et identifier le premier étage noir par readback réduit.
3. Pour l'avion blanc, faire un A/B strict `ac6_fix_deswizzle=false` et observer
   directement format/swizzle, constantes, cible et blend ; ne pas activer le
   fetch invalide global puisque le fetch défectueux sélectionné est valide.

# Cycle 760 — batch mapobj Mission 01 qualifié

Date : 2026-08-04

## Résultat

Le premier raccord environnement natif qualifié est fermé pour les quatre
`mapobj_m01` de l'entry 9 au niveau asset/matériau/texture. Les variantes
actives `mapobj_m01_l_brg1_n` et `mapobj_m01_l_brg2_n` sont en plus reliées à
leurs draws gameplay exécutés, à leurs vertex/texture fetches réels et aux
constantes object-to-clip du VS `C1EE3147DFD5E624`.

Le batch Vulkan générique limité à ces deux variantes actives fait passer le
readback hors HUD de `flight_world_pixels=12` à `141`. Les `129` pixels
modifiés par ce batch sont identiques sur deux exécutions Xvfb successives.
Aucun terrain, skybox, nuage, avion, HUD ou rotation n'a été ajouté.

## Raccords qualifiés

| asset | GIDX MATE→NDXR→NTXR | parties | état runtime |
|---|---:|---:|---|
| `mapobj_m01_l_brg1_n` | 268439850 | 1 | draw 514, frame GPU 9665 |
| `mapobj_m01_l_brg1_b` | 268439851 | 3 | résident, aucun draw observé |
| `mapobj_m01_l_brg2_n` | 268440744 | 1 | draw 468, frame GPU 9664 |
| `mapobj_m01_l_brg2_b` | 268440779 | 3 | résident, aucun draw observé |

Les quatre NTXR portent le profil exact de 24 mots inventorié au cycle 732.
Le décodeur natif accepte uniquement cette signature et la qualifie en BC3
256×256 tiled, conformément aux fetches Vulkan runtime `guest_fmt=20` :

- `brg2_n` : texture `0x045FB000`, image/view hôte non nulles, vfetch
  `0x045E8C40+34240` dans son NDXR ;
- `brg1_n` : texture `0x045A2000`, image/view hôte non nulles, vfetch
  `0x04597990+13952` dans son NDXR.

Le microcode qualifié effectue exactement
`position.x*c218 + position.y*c219 + position.z*c220 + c221`. Le nouveau seam
générique projette donc les meshes avec les constantes exécutées, sans TCAM,
fit-to-clip, pose ou rotation inventée. Les constantes proviennent des lignes
50291–50294 et 50323–50326 de
`reports/logs/cycle-759-entry9-mapobj-transform-runtime/ac6recomp-follow.log`.

Le scan des pointeurs root/12 enfants reste négatif sur huit générations.
L'ownership retenu ici est plus direct : un draw exécuté consomme simultanément
le vertex range du NDXR nommé et la base physique du NTXR exact. Le latch
haut-niveau MATE reste `joined=0`; aucune sémantique de manager n'est inventée.

## Validation

- build RelWithDebInfo : succès ;
- test discriminant NTXR/projection/renderable : 3/3 ;
- CTest PAL complet : 63/63, test SDL dummy skippé comme prévu ;
- Xvfb/X11 Vulkan : deux runs, sortie identique :
  `flight_world_pixels_before_environment=12`,
  `qualified_environment_pixels=129`, `flight_world_pixels=141` ;
- `git diff --check` ciblé : succès ;
- SHA-256 des deux sorties :
  `b4d5ad17173ddcc3efbe2c55c0eb9746df9b34cecf5627133fb5243f16fe0eb4`.

## Identité

- workspace : `442c6dbcd5188fb84b056293a3ce7a000bd20669` ;
- reconstruction parent : `9b893e6fd76e972dbbb6dbef81f1be015b5e6dd9` ;
- runtime/RexGlue vendored : `b8b03c7a89dc7f23bcd7844d15aa5080d480bf11` ;
- XEX : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- DATA.TBL : `82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5` ;
- DATA00.PAC : `c3ed20ec6ef0260671d9cd5f3e088fab2a8d983cb6739efab350c87c6fb74816` ;
- DATA01.PAC : `eddb687418d4b49e36dd8b4e06f387e79be9c0792e97ea3405ab00dab76c03b4` ;
- runtime cycle 759 :
  `937a5d46901e1b6be9b043a9c0d7702077cf57159a441bcf6695b130b8da2c58` ;
- executable SDL cycle 760 :
  `d23b991eb562b9f5a5e1927e5dfd6606c4eff6a954d6e94520bd205b5d3f1d67` ;
- ensemble des sept sources de reconstruction modifiées :
  `b1a90b3fa6fcc060a2cbf9febb073e2557984bb45689740989de4b8dadf88495` ;
- diff suivi runtime préservé :
  `5959cb96839475c9f21b1ed632062ded35fee11bea0c519641843d44a8a9643b`.

Hôte Vulkan inventorié : NVIDIA RTX PRO 4000 Blackwell, pilote propriétaire
595.84, Vulkan 1.4.329. Le test utilise le backend Vulkan natif, une cible
persistante 640×360 puis la surface/swapchain SDL X11. Aucun Xenia n'a été
lancé. L'unique Ollama partagé (`127.0.0.1:11435`, PID 3585823) est resté
intact; aucun processus d'un autre projet n'a été touché.

## Limites et prochaine frontière

Le run runtime cycle 759 déclarait `ac6_unlock_fps=true`. Son raccord de draw
reste une observation directe, mais il ne vaut pas baseline stock-timing ; le
prochain oracle runtime devra passer explicitement
`--ac6_unlock_fps=false` et vérifier le readback cvar.

Les variantes `_b` ne doivent pas être rendues avant observation de leur
activation/destruction. Terrain, sky/cloud, avion joueur/LOD et caméra de vol
restent non identifiés. Le HUD vert, la TCAM/CUT et le `fit_mesh` Mission 2
restent diagnostiques et ne comptent pas comme gameplay. La classification de
l'avion blanc et du premier stage noir du render graph n'est pas fermée.

# Cycle 732 — diagnostic graphique et inventaire des candidats statiques

Date : 2026-08-04

## Verdict de la capture

La capture présentée n'est pas une frame de gameplay qualifiée. Elle mélangeait
trois choses différentes :

- l'overlay HUD vert de diagnostic, qui ne reproduit pas le HUD retail ;
- une caméra TCAM de CUT, pas la caméra de vol ;
- pour Mission 2, un mesh `fit_mesh_to_clip` utilisé uniquement comme smoke
  AABB, donc sans pose ni orientation gameplay.

La suppression de la fenêtre SDL cachée a bien rétabli la présentation Vulkan,
mais elle ne pouvait pas créer le monde absent. Les métriques restent :

```text
scene_changed=4439
world_changed=11
textured_changed=1
flight_world_pixels=12
```

Elles prouvent une soumission Vulkan minimale, pas un terrain, une skybox ou un
avion joueur correctement orienté.

## Correctif exécuté

`project_campaign_mesh_clipped` ajoute un chemin de soumission qui découpe
chaque triangle au plan proche. Le contrat strict précédent est conservé pour
les fixtures qui doivent rejeter tout sommet derrière la caméra. Le test dédié
qualifie le cas d'un triangle traversant le plan proche ; la présentation SDL
reste stable après le changement.

Ce correctif ne change pas encore les métriques ci-dessus : la frontière
dominante est maintenant l'absence de l'activation/du lot de monde gameplay,
et non un simple rejet de sommets.

## Validation

Le test SDL/Vulkan sous Xvfb conserve la même sortie qualifiée :

```text
vulkan_campaign_sdl_presented=1 mission=1 hud=1 scene_changed=4439
world_changed=11 textured_changed=1 flight_changed=1 flight_world_pixels=12
mission1_completed=1 mission2_restored=1 mission2_presented=1
mission2_changed=6974
```

Le CTest PAL complet passe `63/63` avec le smoke SDL explicitement ignoré sous
le driver dummy; la même cible SDL passe sous Xvfb. Le script Python
metadata-only passe `py_compile` et reproduit `292` NDXR, `4` candidats
statiques et le SHA-256 source ci-dessus.

## Assets statiques trouvés dans l'entrée 9

L'inventaire local de l'entrée décodée (`SHA-256`
`cd81e02189516cb5ba0c08d41659a90ae927fe2eccdad53cf5216db44b6d7a05`) trouve
292 modèles NDXR valides, dont quatre candidats `mapobj_m01` :

| chemin MDLP | offset | taille | SHA-256 | noms NDXR |
| --- | ---: | ---: | --- | --- |
| `root.1.m18.6` | `0x00DE73F0` | 15 481 | `d21989ba9a9d2798c3c6db9f80659ed68198fb1686e8b8b6c2b53454dfd4453c` | `mapobj_m01_l_brg1_n...` |
| `root.1.m18.7` | `0x00DEB070` | 23 833 | `a04c769743837816982086b60540b5984061c546aaec9800f6f6b193b44cf930` | `mapobj_m01_l_brg1_b...`, `bmot_a`, `bmot_b` |
| `root.1.m19.6` | `0x00E38080` | 37 337 | `6e72a49786c492800d3497f763e3c9115d6bc812915bee85456b41d5efb8d978` | `mapobj_m01_l_brg2_n...` |
| `root.1.m19.7` | `0x00E41260` | 36 009 | `686bc6e4eca7e6c219c339d1f36073c2a3354e39a7b17e3354485eab3b695546` | `mapobj_m01_l_brg2_b...`, `bmot_a`, `bmot_b` |

Leur présence est une preuve de contenu statique disponible, pas une preuve de
propriété Scene/CUT ni de transformée de gameplay. Aucun de ces modèles n'est
injecté dans le renderer sans ce raccord.

Commande reproductible, sur un fichier décodé local :

```bash
python3 reconstruction/ace-combat-6/tools/entry9_static_inventory.py \
  /path/to/entry_0009.decompressed.bin \
  out/entry9-static-inventory.json
```

## Suite immédiate

1. fermer le join `mapobj_m01` → map/runtime → transformée et lot de rendu ;
2. identifier la ressource sky/cloud réellement possédée par la scène de vol ;
3. remplacer la caméra TCAM de CUT et le smoke `fit_mesh_to_clip` par une
   caméra/pose de vol qualifiée ;
4. seulement ensuite reconstruire le HUD retail à partir de ses ressources et
   constantes, au lieu d'étendre l'overlay vert de diagnostic.

La capture suivante devra être étiquetée `scene_cut`, `flight_world` ou
`diagnostic_fit`; aucune d'elles ne sera appelée gameplay avant fermeture de
ces trois joins.

Le harness ne retient désormais plus Mission 2 par défaut. Pour une capture
post-présentation, `AC6_SCREENSHOT_STAGE=mission1`, `flight` ou
`mission2-diagnostic` doit être demandé explicitement; ce dernier nom rappelle
que son mesh est encore `fit_mesh_to_clip` et ne doit pas servir de preuve
d'orientation de vol.

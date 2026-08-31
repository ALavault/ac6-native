# AC6 natif NTSC-U/J — N1b M01-B formelle (2026-08-30)

## Décision

N1b est verte. Deux processus Linux natifs distincts exécutent exactement
3 600 ticks de Mission 01. Le script N1a reste inchangé pendant les 1 800
premiers ticks, puis le vol neutre se poursuit jusqu'au tick 3 600. Les deux
replays, les huit captures Vulkan et les reçus sont identiques octet par octet.
Les deux processus terminent avec le statut 0 et libèrent proprement leur
fenêtre, surface, swapchain, device et backend Vulkan.

## Fermeture scène et JV

L'audit statique a montré que `free_flight_world_complete` vérifiait déjà les
ressources store-backed NTSC-U/J, tandis que `complete_render_scene` et
`jv_eligible` restaient à zéro par initialisation. La promotion est maintenant
fail-closed et séparée en deux étapes :

- `complete_render_scene` dérive du contrat monde NTSC-U/J exact : 4 226
  placements cité, 65 536 cellules terrain, eau visible et dessinée, F-16
  retail 4 435 sommets/6 468 indices ;
- `jv_eligible` n'est produit dans le reçu qu'après 3 600 ticks, huit snapshots
  gameplay valides, huit images Vulkan contrastées avec HUD vert, progression
  sémantique/visuelle et digest replay final.

Le reçu v2 rapporte `free_flight_world_complete=true`,
`complete_render_scene=true`, `jv_eligible=true`, `five_controls_visible=true`
et `eligible=true`. Aucun marker, caméra diagnostique, intégrateur générique,
raster CPU interactif, signal terminal ou état synthétique n'intervient.

## Déterminisme et visibilité

- replay commun :
  `6bf324b84b9949e2aeb2f189302234f2d7c368997c645d96efc2962e30c6fead` ;
- reçu commun :
  `90fcddb912cf7ef8b5abad0e30c8a3d9ddcdd8e77c42d4f6e1390703f61caf39` ;
- `cmp=0` pour replay, reçu et répertoires contenant les huit PPM ;
- dernier jalon : tick 3 600, `sustained-flight`, effet sémantique et visuel
  vrais, 112 pixels HUD verts, luminance 0–255.

## Validation

- build ciblé `ac6-native`, input et scène Vulkan : vert ;
- tests ciblés : verts ;
- scène NTSC-U/J explicite : `complete=1`, 4 226 draws runtime, 4 740 meshes,
  169 textures, 65 536 draws terrain ;
- CTest complet : 88/88, zéro échec, 19 skips explicites sans fixtures retail ;
- cache : 926 blobs, 15 missions, 5 409 550 519 octets, index
  `d70719285296f3944c9a5e66ac21efc43298efc7497e08eed8b9ff2c1a34df5b` ;
- frontière produit : 266 sources, un ELF, zéro staging interdit.

Artefacts principaux :

- `artifacts/native-us-n1b-runs-20260830/run-a/receipt.json` ;
- `artifacts/native-us-n1b-runs-20260830/run-b/receipt.json` ;
- `artifacts/native-us-n1b-runs-20260830/run-a.replay` et `run-b.replay` ;
- `artifacts/native-us-n1b-runs-20260830/comparison.log` ;
- `artifacts/native-us-n1b-runs-20260830/ctest-final.log` ;
- `artifacts/native-us-n1b-build-20260830/us-scene.log` ;
- `artifacts/native-us-n1b-runs-20260830/cache-audit.log` ;
- `artifacts/native-us-n1b-runs-20260830/product-audit.log`.

## Frontière suivante

N2 doit charger les données armes/durabilité US et relier naturellement
loadout, arme, tir, dégâts, destruction, compteur et gardes scheduler jusqu'au
premier objectif. Aucun signal `-2`, compteur synthétique ou état terminal
forcé ne peut satisfaire ce gate. PAL, M02–M15, succès global, débrief, niveau
2 et save/reload restent gelés.

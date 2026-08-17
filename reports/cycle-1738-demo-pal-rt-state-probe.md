# Cycle 1738 — état RT0/resolve brut du neutral PAL

## Résultat

Une exécution neutral fraîche, store copié neuf, codegen ON, Vulkan/Xvfb et
253 ticks a activé uniquement `AC6_DEMO_WATCH_RESOLVE=1`. Le binaire est
`/fastdata/lavaulta/tmp/ac6-viewport-probe.1KCpUW/ac6-demo-recomp`, SHA-256
`89ae75b98145d46407db70909529fcb269055bfdb62d7e2027ae04a56966a27b`. Le
processus termine avec `return_code=0`.

Un contrôle START frais, avec le même binaire et la même sonde, reproduit les
mêmes lignes `AC6_RECT_STATE` et le même stdout renderer (SHA
`933597e9…e2c0`); son stderr est byte-identique au neutral
(`3df64ed2…6c884`). Cette A/B ne qualifie toujours aucun pixel, mais écarte une
différence de route dans cet état graphique.

Le reçu durable est
[`ac6-demo-pal-rt-state-probe-v1.json`](../analysis/demo/ac6-demo-pal-rt-state-probe-v1.json).
Les traces brutes restent sous
`/fastdata/lavaulta/tmp/ac6-cycle1738-raw.ou1mcX/` et ne sont pas suivies.

## Faits démo observés

| étape | `RB_SURFACE_INFO` brut | `RB_DEPTH_INFO` | état copy brut |
|---|---:|---:|---|
| draw normal, tick 0 | `0x0A020280` | `0x000102D0` | `RB_COPY_CONTROL=0` |
| draw resolve, tick 1 | `0x14000500` | `0x000102D0` | base `0x1374A000`, pitch `0x02D00500`, info `0x01000300`, control `0x00100000` |

Les quatre `RB_COLOR_INFO` valent zéro dans les deux snapshots. Les viewport
bruts sont `640, 640, -360, 360` en IEEE-754; le masque couleur est
`0xFFFF` au draw normal et `0x000F` au draw resolve. La sonde voit 5 shaders,
26 draws, 1 PRESENT et les readbacks noirs déjà établis
(`0b150fd3…ec58366` et `0c660f2b…a4913a5f`).

## Décodage générique séparé

Avec les bitfields ReXGlue/Xenia, sans les promouvoir en preuve AC6 :

- normal : pitch 640 pixels, MSAA `k4X`, hi-Z pitch 640;
- resolve : pitch 1280 pixels, MSAA `k1X`, hi-Z pitch 1280;
- depth base raw 720 tiles, depth format raw 1;
- copy : source 0, sample 0, commande générique `kConvert`;
- destination : pitch 1280, hauteur 720, format raw 6, endian128 0,
  `swap=1`, adresse observée `0x1374A000`.

Le screen-scissor générique lu par la sonde est `[0,0]–[8192,8192]`;
la window-scissor active n’était pas capturée dans ce probe. Ces valeurs sont
des décodages d’architecture, pas des noms sémantiques PAL.

## Qualification et prochain checkpoint

Ce cycle ferme la question « quelle transition de surface précède le resolve »
au niveau des mots de registres, mais ne ferme pas la source EDRAM : aucun octet
non nul ni writer guest n’est encore établi. Le prochain test ciblé doit tracer,
sur un run neutral frais, le premier store dans la plage EDRAM correspondant à
RT0 avant le `RB_COPY`, avec PC/LR guest, thread et tick; il doit rester
transactionnel et piéger sur toute adresse ou longueur inconnue.

La sonde est opt-in, le chemin par défaut est inchangé, aucun fallback visuel
ou screencap n’est qualifié, et aucune preuve retail/Xenia n’est fusionnée.

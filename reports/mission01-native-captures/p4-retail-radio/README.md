# P4 — radio retail M01

Cette slice ne qualifie que le chemin natif d’un événement radio de démarrage.
La clé et l’identifiant viennent du leaf `DATA.TBL[34]` `root/0015` :

- `JIKKYOU_PLAYER_AWACS_MISSION_START` : id `15` ;
- leaf externe : `size=5944`, SHA-256
  `2c5d9fe0ca271e2869157cfc14fdaffa1988d5152275dbdf1647a0b3578b0fd0` ;
- racine entry 34 : `ce5316ffe7f2e52a17bcd7c218a74303fb911a7240fef16b33b5ea416301b0f0` ;
- le même root contient les noms `mapobj_m01_l_brg1/brg2`, avec une fermeture
  statique sans parser note.

Le manifeste demande au runtime natif de jouer l’événement à l’ouverture de
la mission. Il ne prétend pas qualifier les timings retail des événements
`SHTDWN_SHIP_DESTROYER` (id 86) ou `MISSION_END` (id 98), ni une voix XMA
exacte ; aucun payload retail n’est commité.

`color.png` et `object-id.png` sont les readbacks natifs exportés en PNG.
`depth-preview.png` est une vue inversée 8-bit du `depth.f32` normalisé et
`wireframe.png` une vue de contours dérivée du color readback ; ces deux
derniers sont des exports d’analyse et ne constituent pas des passes produit.

## Captures hashées

| fichier | SHA-256 |
| --- | --- |
| `color.png` | `c585cd33f67aa20c2a66c7ca3513b05950e163fde8cad64d23993b4062091b5d` |
| `depth-preview.png` | `77767f35cedef0687b00910988379b0882f3ae4b2d0a79178dff7fe5a51fa8d0` |
| `wireframe.png` | `d74b3fb49972e5a24b7c38cbc9e33c72d117522a906f9655b3adf86c4cca1aba` |
| `object-id.png` | `d74af7220e79dfa2a0ef8c0bad5d79644361e5ebd083ce7ac05fc4f526299b58` |
| `capture-metrics.json` | `4f95d78f4b4c1649647c8a2cf69f4fdc08fc5fcc25d3019e8fa158f7b97a136a` |
| `native-session.json` | `c80bebca9624bceb407f4d5162684fa04392b197e09cd3bb82a5cdc7a0465f71` |

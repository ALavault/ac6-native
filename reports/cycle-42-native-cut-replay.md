# Cycle 42 — replay natif AC6 de la frontière CUT

## Identité et portée

- XEX PAL : `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- données retail lues localement, jamais versionnées ;
- route native bornée : sélecteur de campagne `1` -> ressource DPL `9` ->
  entrée `DATA.TBL` `9` -> groupe Scene `22.1.0` -> CUT de 120 échantillons.

## Exécution

```sh
cmake --build .build/ace-combat-6 -j16
ctest --test-dir .build/ace-combat-6 -j16 --output-on-failure
SDL_VIDEODRIVER=dummy .build/ace-combat-6/ac6-scene-shell \
  --campaign-selector 1 \
  workspaces/ace-combat-6/game-files/DATA.TBL \
  workspaces/ace-combat-6/game-files/DATA00.PAC \
  workspaces/ace-combat-6/game-files/DATA01.PAC \
  --play-to-completion
```

Le build et le corpus passent **38/38**. La sortie structurée du replay a pour
SHA-256 `b2d31adf16cee08060f03138ffa239d296acdb3755edf2f2013df046372bf52e`
et affirme :

- `campaign_resource_route_proven:true` ;
- `native_scene_playback_phase:"complete"` à l'échantillon `119` ;
- `native_campaign_scene_session_phase:"scene_complete"` ;
- 120 états caméra, 16 objets CUT joints (14 `Rigid`, 2 `AnimRigid`) ;
- 39 393 sommets, 57 271 indices et 115 polygones texturés présentés par le
  lecteur natif borné.

## Limites et prochaine action

Ce résultat est une reconstruction native déterministe de données sérialisées,
pas une exécution du code Xenon ni une validation Xenia. Il ne prouve ni
activation de mission, ni spawn, ni vol, ni HUD retail, ni transition après le
CUT. La prochaine preuve pour la frontière `0x821b9408` reste une observation
Xenia/XenonTests des cinq retours clé-vers-handle et des écritures objet.

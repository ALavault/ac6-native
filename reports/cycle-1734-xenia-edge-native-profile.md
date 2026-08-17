# Cycle 1734 — Xenia Edge natif, profil persistant (oracle)

## Identité

- Dépôt : [has207/xenia-edge](https://github.com/has207/xenia-edge)
- Release/tag : [`60ff861`](https://github.com/has207/xenia-edge/releases/tag/60ff861)
- Commit du tag : `60ff8616696e81726f09053874c12adc7716537f`
- AppImage : `.tools/xenia-edge-60ff861/xenia_edge_linux.AppImage`
- SHA-256 AppImage : `c2cac2a029ce0d44a71c4e919fd71c702654079023b63fd669472ba3cd78b828`
- Cible AC6 concernée : démo PAL `Default.xex`, SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.

## Vérifications

- L’AppImage est un ELF Linux x86-64 natif (BuildID
  `804289becc7fc6672c279ca50542e32508c1e518`).
- Le runtime AppImage signale l’absence de FUSE dans l’environnement courant;
  le launcher utilise donc `--appimage-extract-and-run`, sans modifier
  l’AppImage.
- Les sources Edge épinglées exposent `--storage_root`, `--content_root`,
  `--cache_root` et `--logged_profile_slot_0_xuid`.

## Correction du profil

Le launcher [`scripts/run_xenia_edge_native.sh`](../scripts/run_xenia_edge_native.sh)
fixe par défaut :

```text
storage_root = .tools/xenia-edge-profile
content_root = .tools/xenia-edge-profile/content
cache_root   = .tools/xenia-edge-profile/cache_host
```

Edge recherche ensuite les comptes dans
`content/<XUID>/FFFE07D1/00010000/<XUID>`. Le profil n’est donc créé qu’une
fois dans cette racine; les lancements suivants réutilisent le compte et la
configuration. Le `ProfileManager` Edge persiste aussi
`logged_profile_slot_0_xuid` lors d’un login/logout, ce qui évite de refaire la
sélection manuellement. `XENIA_EDGE_PROFILE_ROOT` permet une autre racine
persistante. `XENIA_EDGE_PROFILE_XUID` peut fixer le slot 0 après création,
mais n’est pas deviné.

## Limites

- Xenia Edge est une autorité générique/oracle, pas une preuve PAL et ne
  remplace pas les replays `ac6-demo-recomp`.
- Aucun XEX, profil, shader, microcode ou donnée de jeu n’est copié dans le
  dépôt par ce changement.
- La première création du profil reste interactive si la racine persistante
  est vide; elle ne sera plus demandée à chaque démarrage.

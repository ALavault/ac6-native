# Cycle 1057 — frontière factory retail après le handoff gameplay

Date : 2026-08-06.

## Classification

Cette expérience est `bridge` uniquement. Elle ne peut pas valider un gate
native. Aucun payload retail, buffer de rendu ou capture bridge n'est ajouté au
dépôt ; seuls leurs hashes et les observations bornées sont conservés ici.

## Provenance

- XEX PAL `default.xex` : SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Source bridge externe : commit `b8b03c7a89dc7f23bcd7844d15aa5080d480bf11`,
  arbre préexistant dirty.
- Binaire bridge : SHA-256
  `c94d00106afb23d105c5b3f1c24715124c1e62d01c39dd0b45cae7ffd4153a75`.
- GPU : NVIDIA RTX PRO 4000 Blackwell, Vulkan.
- Audio : `SDL_AUDIODRIVER=dummy`.
- Run : `/tmp/ac6-cycle-1057-retail-unit-factory`.
- Follow log : SHA-256
  `e16391e59c9bd053c694c730056bf719fc06afb58f478c7a5969f398435838f4`.
- Launcher log : SHA-256
  `b4ea4bb0702d10bea4be1326b8b1b1f57882b32731a084a79bf84d85bb04f654`.

Le profil utilisateur était neuf. La recette `scripts/ac6-first-mission.steps`
a été lancée avec `--capture-at 0`, puis les confirmations de loadout et de
déploiement ont été envoyées après l'écran d'observation. Le log contient
16 738 `PRESENT`, 842 enregistrements de task Mission 01 et 10 transitions de
campagne.

## Transition et observation retail

La route courante atteint :

```text
type28=30 → type28=37 → type28=35 → selector44=3
→ selector44=4/type28=6 → selector44=7/type28=8
→ selector44=8/type28=10 → campagne state 1→2
```

La capture bridge du briefing affiche les textes retail exacts
`Invasion of Gracemeria` et `Aerial Defence (Air-to-Air)`.

- `manual-deploy-10s.png` : SHA-256
  `aa5d0a7f4591c1838c2aee2bc1f908133caca6b8a9e3f57efc7df5868c7cd492`.
- `manual-cinematic-45s.png` : SHA-256
  `944c44273338b738d896a9eac7efc8cd185173826d64dba9c0f2676164be25c85`.
- `manual-now.png` : SHA-256
  `e8f39f8da62f4650173965e94e68cdde68f21198eea1483a902f5a0382f0b1ec`.

Ces images sont des captures bridge. Le texte est une observation retail
utile pour la future liaison de présentation, mais ne fournit ni identifiant
de table, ni condition, ni cible, et ne passe donc pas `retail_objectives`.

## Factory et registre

Le hook read-only `0x820A7F48` est enfin appelé au frame 11761 :

- 128 appels `enter` et 128 appels `leave` dans la fenêtre bornée ;
- `selector=1` retourne le joueur `0x820568D4` ;
- `selector=3` retourne `0x82009AB0` ;
- 126 appels `selector=4` retournent des objets au vtable `0x82009440`.

Le census `0x822707C8` au frame 11771 puis pendant le gameplay observe :

- `object_count=230` ;
- joueur `0xB2470000`, vtable `0x820568D4` ;
- enfant joueur `0xB2470100`, vtable `0x82007A10` ;
- `other_player_count=0` ;
- histogramme `0x820568D4:1`, `0x82009AB0:1`, `0x82009440:228`.

Les appels `ac6-gameplay-tick` progressent ensuite avec `UpObj`, `UpCam`,
`UpRadio` et `player_update`, mais aucun événement de registre d'unité,
publication de vague, faction, cible ou transition d'objectif n'est présent.
Le vtable `0x82009440` reste donc anonyme. L'association de ces 228 objets à
des ennemis par leur ordre ou leur vtable est explicitement rejetée.

## Décision

La frontière storage/handoff et l'entrée factory sont acquises, mais la preuve
ne ferme pas l'identité retail des unités ou des vagues. Les gates natifs
`units_and_waves` et `retail_objectives` restent ouverts ; aucune modification
du renderer, des textures ou de la caméra n'est justifiée par ce run.

# Cycle 733 — handoff de redémarrage vers Mission 01 jouable 1:1

Date : 2026-08-04T03:49:02+02:00

## Cible

Le but long terme reste une version native d'Ace Combat 6, indépendante de
RexGlue, Xenia et des bibliothèques Xbox, avec Vulkan comme backend. La cible
immédiate est une Mission 01 réellement jouable et visuellement fidèle : monde,
terrain, ciel, avion joueur, HUD, commandes et logique de mission issus des
assets et contrats retail qualifiés. Une frame diagnostique visible ne compte
pas comme parité.

Le `/goal` précédent est terminé. Il a fermé le chemin STANDBY/entrée, le
chargement DPL/PAC, une progression native synthétique, la sauvegarde `AC6S` et
le déverrouillage de Mission 2. Il n'a pas fermé la parité graphique ou la
jouabilité retail de Mission 01.

## Identités qualifiées

```text
Target:             Xbox 360 PAL Ace Combat 6
default.xex size:   7 483 392
default.xex SHA256: acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde
DATA.TBL size:      14 824
DATA.TBL SHA256:    82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5
DATA.TBL records:   926
pack count:         2
asset root:         /fastdata/lavaulta/auto-re-agent/workspaces/ace-combat-6/game-files
```

Les PAC et packs retail restent locaux et ne doivent pas être ajoutés au
dépôt. Les extractions bornées et leurs métadonnées/hashes sont autorisées.

## État vérifié à la coupure

- La fenêtre SDL/Vulkan est réellement visible après `SDL_ShowWindow()`.
- L'observation utilisateur exige `A` sur STANDBY puis encore `A` sur l'écran
  suivant pour atteindre la mission. Le runtime natif possède le masque A
  `0x1000`, mais sa fixture simplifie encore le chemin de lancement.
- Les axes SDL atteignent l'état de vol natif et modifient la projection.
- La présentation courante n'est pas du gameplay : HUD vert diagnostique,
  caméra TCAM de CUT et, pour Mission 2, mesh `fit_mesh_to_clip` sans pose de
  gameplay.
- Métriques Mission 01 inchangées : `scene_changed=4439`,
  `world_changed=11`, `textured_changed=1`, `flight_world_pixels=12`.
  Elles prouvent une soumission minimale, pas un monde visible.
- Le groupe Scene 0 de l'entry 9 fournit 16 objets animés/rigides de cutscene,
  notamment deux avions et des véhicules. Il ne joint ni terrain ni skybox.
- `project_campaign_mesh_clipped()` découpe désormais les triangles au plan
  proche. Le correctif est valide mais ne change pas les métriques : le monde
  absent n'était pas un simple rejet near-plane.
- L'inventaire metadata-only de l'entry 9 décodée
  (`cd81e02189516cb5ba0c08d41659a90ae927fe2eccdad53cf5216db44b6d7a05`)
  trouve 292 NDXR et quatre candidats `mapobj_m01_l_brg*`. Leur propriété
  gameplay et leurs transforms ne sont pas encore prouvés.
- Le harness ne retient plus Mission 2 implicitement. Les seules étapes de
  capture admises sont `mission1`, `flight` et `mission2-diagnostic`; la
  dernière reste explicitement un `fit_mesh_diagnostic`.

Rapport source : `reports/cycle-732-graphical-frontier.md`.

## Changements à préserver

Source native :

- `reconstruction/ace-combat-6/include/ac6/camera_projection.h`
- `reconstruction/ace-combat-6/src/camera_projection.cpp`
- `reconstruction/ace-combat-6/tests/camera_projection_tests.cpp`
- `reconstruction/ace-combat-6/tests/campaign_vulkan_sdl_present_tests.cpp`
- `reconstruction/ace-combat-6/tools/entry9_static_inventory.py`

État durable :

- `workspaces/ace-combat-6/CURRENT_PLAN.md`
- `workspaces/ace-combat-6/NATIVE_RECONSTRUCTION_STATUS.md`
- `workspaces/ace-combat-6/reports/cycle-732-graphical-frontier.{md,json}`

Le monorepo est très sale et `reconstruction/ace-combat-6/` apparaît comme un
répertoire non suivi. Ne pas nettoyer, reset ou écraser les changements
existants. Ne jamais modifier une sortie générée.

## Validation reproduite

```bash
cmake --build \
  /fastdata/lavaulta/auto-re-agent/.build/ace-combat-6/reconstruction-material \
  -j16

AC6_ASSET_ROOT=/fastdata/lavaulta/auto-re-agent/workspaces/ace-combat-6/game-files \
ctest --test-dir \
  /fastdata/lavaulta/auto-re-agent/.build/ace-combat-6/reconstruction-material \
  --output-on-failure
```

Résultat final : `63/63`, avec le test SDL/surface explicitement skipped sous
le driver dummy.

Validation de la vraie surface :

```bash
AC6_ASSET_ROOT=/fastdata/lavaulta/auto-re-agent/workspaces/ace-combat-6/game-files \
AC6_SCREENSHOT_STAGE=flight AC6_SCREENSHOT_HOLD_MS=100 \
xvfb-run -a -s '-screen 0 640x360x24' \
  /fastdata/lavaulta/auto-re-agent/.build/ace-combat-6/reconstruction-material/ac6-campaign-vulkan-sdl-present-tests
```

La sortie passe et contient maintenant
`diagnostic_hud=1 mission2_frame_class=fit_mesh_diagnostic`.

Inventaire metadata-only reproductible :

```bash
python3 /fastdata/lavaulta/auto-re-agent/reconstruction/ace-combat-6/tools/entry9_static_inventory.py \
  /path/to/entry_0009.decompressed.bin \
  /tmp/ac6-entry9-static-inventory.json
```

## Processus à la coupure

Après autorisation explicite, les arbres anciens suivants ont été arrêtés
proprement avec SIGTERM et leur disparition a été vérifiée :

```text
1305095/1305176/1305177  pipeline ac6-scene-shell
1909667/1909726/1909734  pipeline de capture Xvfb :98
```

`Xvfb :106` (PID observé `2141405`) appartient à une exécution Pharaoh hors
scope AC6 et est resté intact. La prochaine session doit réinventorier les
processus avant toute action; aucun processus AC6 connu n'est attendu.

## Plan de reprise par checkpoints

1. **Ownership du monde** — partir des parents MDLP/FHM des quatre
   `mapobj_m01`, qualifier leur resource ID, leur map/runtime owner et leur
   transform; rechercher de la même façon sky/cloud/terrain. Produire un graphe
   metadata-only exact. Succès : au moins un batch environnement possède
   asset, material/texture, transform et consumer runtime prouvés.
2. **Batch environnement Vulkan** — joindre ce batch au loader générique,
   respecter profondeur, culling, alpha et topologie, puis mesurer sa couverture
   hors HUD. Succès : croissance reproductible de `flight_world_pixels` avec
   un identifiant asset qualifié; aucune géométrie synthétique.
3. **Avion et caméra gameplay** — qualifier le spawn, le LOD joueur et la pose
   caméra de vol, puis supprimer l'usage d'une caméra CUT ou d'un fit AABB dans
   toute capture appelée gameplay. Succès : orientation/cadrage dérivés de
   données retail et test de mouvement pitch/roll/yaw/throttle.
4. **HUD retail** — identifier les ressources, constantes et producteurs
   réels; remplacer l'overlay vert seulement après ownership. Succès : le HUD
   diagnostique est désactivé dans la frame gameplay et ses éléments réels
   suivent l'état de mission.
5. **Mission 01 complète** — connecter objectifs, ennemis, collisions, dégâts,
   audio et fin de mission au runtime générique. Rejouer la séquence A/A et un
   parcours déterministe jusqu'au résultat, sauvegarde comprise. La parité 1:1
   doit être mesurée par artefacts nommés, pas déclarée à partir d'un smoke.

Limiter les runs oracle : commencer par assets, code PAL qualifié et replay
déterministe local. Si une frontière statique résiste, demander un unique run
borné avec input exact, artefact attendu et limite de temps; Xenia reste oracle,
jamais dépendance ou preuve de parité native.

## Prompt prêt pour la prochaine session

```text
Reprends Ace Combat 6 uniquement dans le thread principal. Lis d'abord
AGENTS.md, /home/lavaulta/.codex/RTK.md, puis
workspaces/ace-combat-6/reports/cycle-733-session-restart-handoff.md et
workspaces/ace-combat-6/reports/cycle-732-graphical-frontier.md. Préserve le
worktree sale et les changements non suivis; ne modifie aucune sortie générée.
Avant de lancer ou arrêter quoi que ce soit, inventorie les processus AC6,
Xvfb, Xenia et Ollama et ne touche jamais à un processus sans ownership.

Objectif de reprise : avancer par checkpoints vers une Mission 01 native
Vulkan jouable 1:1. Commence par fermer, par preuves metadata/static/runtime,
l'ownership des quatre mapobj_m01 de l'entry 9 et des ressources terrain,
sky/cloud, ainsi que leurs transforms et consumers. Implémente ensuite le
premier join environnement générique réellement qualifié et mesure sa
couverture hors HUD. Ne fabrique ni terrain, ni skybox, ni rotation d'avion, ni
HUD; ne présente jamais mission2 fit_mesh ou une caméra CUT comme gameplay.
N'utilise aucun sous-agent. Évite l'oracle sauf frontière statique nommée
impossible à fermer; dans ce cas prépare un seul run borné au lieu de le lancer
à répétition. À chaque checkpoint : test ciblé discriminant, build, CTest PAL,
Xvfb seulement si nécessaire, rapport md/json et mise à jour CURRENT_PLAN et
NATIVE_RECONSTRUCTION_STATUS. Le premier succès attendu est un batch monde
avec asset + material/texture + transform + owner runtime prouvés et une hausse
reproductible de flight_world_pixels hors HUD.
```

## `/goal` proposé

```text
/goal Rendre la Mission 01 d'Ace Combat 6 jouable 1:1 dans le runtime natif Vulkan, sans RexGlue/Xenia ni correctif synthétique : qualifier et joindre terrain/skybox/avion/LOD/caméra/HUD/audio/assets retail, reproduire la séquence STANDBY A puis A, les contrôles, ennemis, objectifs, dégâts, fin et sauvegarde par checkpoints déterministes avec manifests, hashes, tests et captures nommées.
```

# Pipeline vision et contrôle AC6

## Statut

`tools/ac6_vision_control.py` fournit le moteur déterministe de décision :
options manette bornées, archive Go-Explore, politique UCB, exploration de
nouveauté, prompts de comportement compilés, budgets, trace chaînée et movie
de replay. La cible est exclusivement la démo PAL de SHA-256
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.

Xenia Edge reste un oracle. Son AppImage actuelle n'expose pas encore le
boundary synchrone requis. Il est interdit de remplacer ce boundary par un
`sleep` et de qualifier ensuite le résultat comme frame-exact.

## Contrat du bridge Xenia

Le bridge doit exécuter atomiquement la séquence suivante :

1. terminer un `XE_SWAP` ;
2. suspendre la progression des vCPU invités ;
3. capturer le framebuffer et les métadonnées invitées ;
4. écrire une observation JSONL sur stdin du moteur ;
5. lire exactement une action JSONL sur stdout ;
6. injecter l'état manette au prochain poll XAM ;
7. avancer exactement `hold_frames` frames invitées, ou s'arrêter plus tôt si
   `stop_on_visual_change` est supporté ;
8. revenir à l'étape 1.

Capacités obligatoires avant un run qualifié :

```text
pause_after_completed_present = true
exact_guest_frame_step = true
guest_controller_injection = true
framebuffer_capture_while_paused = true
checkpoint_restore = true
```

Le bridge publie ces capacités dans un JSON
`ac6-xenia-agent-bridge-capabilities/v1`, avec son nom/commit, les boundaries
`completed_xe_swap` et `guest_xam_poll`, puis les cinq booléens ci-dessus. Le
moteur refuse de démarrer si un seul manque ou vaut faux. L'AppImage Edge
`60ff861` n'expose pas encore cette interface et ne doit donc pas recevoir un
faux manifeste positif.

Une observation minimale est :

```json
{"schema":"ac6-agent-observation/v1","identity":{"xex_sha256":"de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8"},"run_id":"run-1","guest_frame":120,"guest_tick":240,"image":{"path":"frames/00000120.png","sha256":"<64 hex>"},"checkpoint":"checkpoints/00000120","state":{"situation":"title","confidence":0.99,"visual_signature":"<stable signature>","ocr_text":"PRESS START"}}
```

Les situations admises sont `boot`, `splash`, `title`, `attract_movie`,
`menu`, `loading`, `gameplay`, `pause`, `mission_result`, `retry` et
`unknown`. Le classifieur amont doit publier `unknown` sous son seuil de
confiance ; le moteur choisit alors une option bornée et ne fabrique pas de
sémantique.

## Utilisation

Compiler un prompt en configuration auditable :

```sh
export TMPDIR=/fastdata/lavaulta/tmp
python3 workspaces/ace-combat-6/tools/ac6_vision_control.py \
  compile-behavior "Explorer tous les sous-menus sans tirer" \
  --output "$TMPDIR/ac6-behavior.json"
```

Lancer le moteur derrière le bridge :

```sh
python3 workspaces/ace-combat-6/tools/ac6_vision_control.py serve \
  --behavior workspaces/ace-combat-6/config/ac6-agent-behavior-explore-menus-v1.json \
  --vision-rules workspaces/ace-combat-6/config/ac6-agent-vision-rules-v1.json \
  --capabilities "$TMPDIR/xenia-agent-bridge-capabilities.json" \
  --archive "$TMPDIR/ac6-agent-archive.json" \
  --trace "$TMPDIR/ac6-agent-trace.jsonl" \
  --seed 2758
```

Compiler ensuite la découverte en movie indépendant de la vision et du HID :

```sh
python3 workspaces/ace-combat-6/tools/ac6_vision_control.py replay \
  "$TMPDIR/ac6-agent-trace.jsonl" \
  --output "$TMPDIR/ac6-agent-movie.json"
```

Le bridge de replay doit ignorer caméra, OCR, modèle et HID, puis appliquer
uniquement les états manette et durées du movie. Deux restaurations fraîches du
même checkpoint doivent produire les mêmes hashes d'observation.

Après la première observation, chaque observation doit aussi joindre le reçu
de l'action précédente : `action_id`, nombre exact de frames avancées et, pour
une restauration, identifiant du checkpoint. Le moteur refuse une action
perdue, dupliquée, raccourcie sans changement visuel ou restaurée au mauvais
checkpoint.

## Récompense et exploration

La récompense intégrée combine nouvelle cellule, changement visuel, situation
préférée, résultat de mission et terminal. UCB estime chaque option par cellule
et pénalise son risque explicite. Les prompts ne commandent jamais directement
les axes : ils deviennent des options autorisées/interdites/forcées, des poids
et des budgets vérifiables.

En mode exploration, `restore_interval` demande périodiquement au bridge de
restaurer une cellule sous-explorée disposant d'un checkpoint. Ces opérations
sont conservées dans le movie et restent rejouables sans modèle.

Les campagnes tordues utilisent `forced_options` et un budget séparé. La
déconnexion virtuelle est disponible mais fortement pénalisée et absente de la
configuration menu fournie.

## Frontières de preuve

- Une trace de découverte explique comment une route a été trouvée.
- Le movie prouve seulement les entrées demandées.
- Les captures Xenia sont `oracle-observed`, jamais des pixels natifs de
  `ac6-demo-recomp`.
- Toute promotion démo nécessite une jointure indépendante aux bytes PAL et à
  un replay déterministe.

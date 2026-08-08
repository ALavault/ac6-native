# AC6 — plan revu vers la première mission

> **SUPERSEDED — 2026-08-08, cycle 1143.** Its Portes A–D also target the recompiled runtime, and its terminal token `AC6_FIRST_MISSION_REACHED` was never reached on that route. The first mission now runs on the native product and is certified by JF instead. Last touched at cycle 514.
>
> The live roadmap is **`MISSION01_LADDER.md`**. The gates that actually run are
> `analysis/contracts/mission01-final-gate-v3.json` (JF) and
> `analysis/contracts/mission01-native-gate-v2.json` (J0/J1), audited by
> `tools/audit_ac6_mission01_native_gate.py`. Working rules are in `CLAUDE.md`.
>
> This file is kept for its history. Do not plan from it.

Date : 2026-08-01. Cible : XEX PAL
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

## Résultat accepté

La borne est franchie seulement si une exécution native unique et bornée :

1. atteint le titre et accepte une entrée ;
2. traverse les dialogues de données sans écriture forcée d'état invité ;
3. sélectionne `Campaign > New Game > Normal > Normal > English` ;
4. sort de la cinématique d'introduction ;
5. charge les écrans de mission puis une scène avec HUD ;
6. montre un changement d'état avion provoqué par une entrée nommée ;
7. ne produit ni `REX_FATAL`, ni signal hôte, ni ressource retail embarquée.

Un menu, une cinématique ou le seul log du pont FHM ne suffit pas. Le token
`AC6_FIRST_MISSION_REACHED` est interdit avant les sept points.

## Review du plan antérieur

Le plan P0–P7 reste utile pour le produit complet, mais il n'est plus le chemin
critique immédiat : le cycle 457 a déjà atteint la cinématique de campagne. La
route verticale doit consommer cette preuve au lieu de reprendre le recensement
des waits du cycle 327.

Trois corrections de méthode sont obligatoires :

- **Entrée de bootstrap immédiate.** Le run réussi v2 enregistre 41 polls XAM
  et tous les fronts de boutons. Les runs gelés v3/v4 n'en enregistrent qu'un ;
  ils cessent de présenter après respectivement 102 et 1852 images. Les touches
  prévues à 35/42 secondes arrivent donc après l'arrêt invité. L'entrée Start
  doit être injectée dès que 30 présentations prouvent le canal vivant, pas sur
  une horloge héritée de l'ancien démarrage.
- **Dialogues pilotés par états.** Les transitions sauvegarde publient des
  types stables `30 -> 37 -> 35 -> 6 -> 8 -> 10`. Les temporisations seules ont
  envoyé `Left` sur l'écran FILE dans v2. Chaque entrée doit attendre le type ou
  le sélecteur attendu dans le journal.
- **Validation du pont avant poursuite.** La sortie de cinématique doit produire
  `[ac6-campaign-resource-bridge]` puis continuer à présenter. Une capture noire
  ou l'absence de crash pendant quelques secondes n'est pas une preuve.

## Plan vertical implémentable

### Porte A — bootstrap vivant

- démarrer sur Xvfb privé 1280×720 avec profil vierge ;
- autoriser 120 secondes à la compilation/cache des pipelines ;
- après 30 présentations, envoyer `Start` immédiatement, puis `A` deux secondes
  plus tard ;
- exiger un poll XAM non nul et l'apparition du type de dialogue 30 dans les
  30 secondes ; sinon classer `runtime-blocked` et conserver la dernière image.

### Porte B — données et menu

- type 30 : `Left`, `A` ;
- attendre puis acquitter 37 et 35 ;
- attendre le sélecteur FILE, envoyer `A` ;
- type 6 : `Left`, `A` ; acquitter 8 puis 10 ;
- capturer le menu principal ;
- envoyer les cinq confirmations campagne avec une capture entre états.

Chaque attente est bornée et vérifie que le processus possède toujours sa
fenêtre. Aucun appui n'est répété sans changement de type/sélecteur ou nouvelle
capture.

### Porte C — sortie de cinématique et chargement

- attendre une capture 3D non noire de la cinématique ;
- envoyer `Start` une fois ;
- exiger le log accepté du pont FHM exact PAL ;
- exiger au moins 120 nouvelles présentations après ce log ;
- capturer et classifier chaque écran suivant, puis envoyer `A` seulement si
  l'écran est stable et interactif.

### Porte D — première mission

- traverser briefing, appareil et lancement par confirmations unitaires ;
- accepter seulement une frame avec monde + HUD ;
- envoyer une entrée de vol bornée ;
- prouver un delta visuel ou un delta d'état joueur qualifié ;
- enregistrer binaire, assets, séquence d'entrée, captures et journal.

## Ordre d'implémentation

1. Corriger le bootstrap du harness existant et ajouter une recette dédiée
   `ac6-first-mission-run.sh` fondée sur les patterns ci-dessus.
2. Tester syntaxe et fonctions de parsing sans lancer le jeu.
3. Exécuter Porte A seule ; ne lancer B que si A passe.
4. Exécuter A+B, puis A+B+C, puis A+B+C+D. Chaque run ajoute une frontière ;
   aucun run long n'enveloppe à l'identique le précédent.
5. Après changement natif, reconstruire `-j16`, exécuter les tests AC6 ciblés
   puis le CTest complet, et mettre à jour le handoff.

## Défauts adjacents

- Le dialogue `PLEASE WAIT` montrant
  `ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789` est confirmé et doit être supprimé ou
  restauré, mais il ne bloque pas les portes A–D.
- Les cinq baselines unitaires chrono/TemplateRegistry et le test PPC
  `vpkd3d128_float16_4_invalid_0` restent hors de cette tranche tant qu'ils ne
  changent pas avec les modifications AC6.

## État après implémentation — cycle 458

- Portes A et B : franchies par la recette déterministe
  `scripts/ac6-first-mission.steps` et son harness
  `scripts/run_ac6_first_mission.sh`.
- Porte C : le crash est corrigé. Le wrapper FHM et le répertoire NTXR vide
  sont acceptés par deux gardes PAL exactes ; 120 présentations supplémentaires
  sont observées sans crash.
- Nouvelle frontière C/D : écran noir vivant après la transition. `A` puis
  `Start` ont été testés séparément et n'altèrent ni l'état visible ni les
  pixels ; ne pas répéter ces essais.
- Porte D : non franchie. Aucun HUD ni delta de contrôle avion n'est prouvé.

Prochaine tranche : instrumenter l'état/mode de campagne, le propriétaire de
scène et la complétion des ressources juste après le walker NTXR, puis comparer
les soumissions GPU de la dernière frame de cinématique à celles de l'écran
noir. Le compte rendu détaillé est
`reports/cycle-458-first-mission-transition.md`.

## État après implémentation — cycle 460

- L'instrumentation de la frontière C/D a identifié une sélection DATA
  incorrecte : le getter de niveau retournait `0`, donc `sub_8218F358`
  choisissait l'entrée 209 au lieu de la première mission en entrée 210.
- Le bridge exact caller/racine/état sélectionne désormais 210 uniquement pour
  `(niveau=0, mode=1, sélecteur=0)`. La scène devient cohérente et la tâche de
  première mission est créée ; l'ancien écran noir n'est plus la frontière.
- La timeline principale passe de la frame 1 à la frame 359 et tous ses
  handlers qualifiés retournent. Le prochain crash survient ensuite à PC hôte
  nul, à la frontière du parcours récursif des timelines enfants.
- Porte D reste non franchie : aucun HUD ni contrôle avion n'est encore prouvé.

Prochaine tranche : au caller PAL `0x8237D0FC` de `sub_8237CC58`, capturer le
premier état enfant, son record candidat, son type et sa cible indirecte, puis
corriger uniquement le contrat invalide. Rejouer ensuite la recette de
jouabilité et exiger monde + HUD, delta d'état avion et intervalle stable. Ne
pas répéter locale, A, Start, le setter persistant de niveau ou GDB.

Compte rendu : `reports/cycle-460-first-mission-package-selection.md`.

## État après reprise — cycle 514

- Le premier record enfant `index=0x10004` franchit désormais
  `sub_8237C4D8` avec une entrée sparse bornée et un offset de record nul.
- Le passage suivant reçoit un nœud retourné dont `+8` est le pointeur invité
  `0xB8EC9438`, pas un index. Son interprétation comme index mène à des données
  ASCII (`No locks available`) et est invalidée.
- L'hexagone de capacités avion rend ses axes mais pas son polygone ; les
  statistiques et quantités restent vides. Ce signal de données loadout est
  conservé sans attribution au child-timeline.
- Porte D reste non franchie : ni HUD, ni contrôle avion, ni stabilité de vol.

Prochaine tranche : qualifier le producteur de `r6` et le compte de répétition
au caller PAL `0x8237CEF0`. Ne pas forcer `child+8`, agrandir la table sparse ou
retester la fausse cible `0x82019E6C`. Compte rendu :
`reports/cycle-514-child-record-return.md`.

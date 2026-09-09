# AC6 retail NTSC-U/J — r479 — piste A (plan approuvé) : le lancement direct sans aucune entrée (hypothèse de r478/r479 pour atteindre l'écran cible) ne l'atteint PAS — capture un jeu de nuanceurs plus précoce et différent, réfutant l'hypothèse

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle utilisé (décision utilisateur explicite : construire et lancer
une nouvelle route ciblée, autorisée en réponse à la question posée
après r478).

## Contexte

Nommé par r478, avec décision utilisateur explicite : les trois motifs
manquants (`09dd1c7cddae1141`, `57b8e5f14b93cff4` avec
`rect_strip_expand` inversé, `4dd456c4ea0923c1` absent) nécessitent une
nouvelle capture oracle atteignant l'écran précis que le sondage natif
zéro-entrée traverse. L'hypothèse la plus directe (nommée dans le
rapport r478 : « probablement une capture très courte, sans entrée du
tout, similaire au comportement du sondage natif lui-même ») a été
testée en premier, avant de construire quoi que ce soit de plus
élaboré.

## Établi — les routes scellées ne permettent pas `--dump_shaders` en dehors du chemin sous scellement

`run_gate.py` couple `--mission-d5b4-final-white` (qui active
`--dump_shaders`) à `--mission-render-summary`, qui exige à son tour
`is_sealed_route()` — donc impossible d'utiliser le chemin
`--diagnostic-route` (non scellé) avec la capture de nuanceurs sans
modifier `run_gate.py` lui-même. Plutôt que de modifier l'outil (un
changement non trivial et non vérifié dans le temps imparti), l'oracle
compilé a été lancé **directement**
(`recompilation/ace-combat-6-retail/install/ntsc-uj/bin/ac6recomp`),
contournant entièrement `run_gate.py`, avec exactement les drapeaux
`--ac6_d5b4_final_white=true --dump_shaders=<dir>` plus les drapeaux
de journalisation/rendu déjà utilisés lors des captures directes de
r467, sous un nouveau serveur Xvfb dédié (`:190`), **sans aucune
entrée synthétique** (ni clavier, ni manette) — reproduisant fidèlement
le comportement du sondage `--probe-entry` natif lui-même.

## Établi — hypothèse réfutée : ceci n'atteint PAS l'écran cible

Lancé ~90 s+ (le `timeout 90` du shell n'a pas terminé le processus au
temps voulu — comportement anormal non investigué plus avant, le
processus a été arrêté manuellement après un temps significativement
plus long). **4 nuanceurs vertex distincts capturés** :
`0a6d1dd7767fdf27`, `472913f460d4b446`, `bbaada3605b82c5a`,
`c049a8c9e556f129` — **AUCUN ne correspond aux trois cibles**
(`09dd1c7cddae1141`, `57b8e5f14b93cff4`, `4dd456c4ea0923c1`). Le
lancement direct sans entrée capture un jeu de nuanceurs plus précoce
et différent (vraisemblablement l'écran de démarrage/logo Namco Bandai
lui-même, avant même l'écran-titre), pas le contenu que le sondage
natif atteint après son propre délai de traitement (r475 : 86 tirages
sur une fenêtre de 30 s, deux salves).

**Ceci réfute l'hypothèse « zéro entrée suffit »** : le produit
oracle-hybride et le produit natif divergent dans leur cadencement de
démarrage (deux bases de code distinctes issues du même binaire
invité, r454+), donc reproduire « aucune entrée » sur l'un ne garantit
pas d'atteindre le même état que « aucune entrée » sur l'autre — le
produit oracle semble simplement plus lent à progresser, ou requiert
une entrée que le produit natif ne requiert pas pour avancer au-delà
du logo.

## Non établi

- **Aucun fichier `.xpso`/`.xsh` de cache persistant n'a été produit**
  — ces fichiers ne sont écrits qu'à l'arrêt propre du processus
  (`SIGTERM` géré ou fermeture normale), pas sur `SIGKILL` (utilisé
  ici après le dépassement du `timeout`). Non investigué : comment
  déclencher un arrêt propre pour une capture aussi courte.
- **Quelle entrée précise (le cas échéant) ferait progresser le
  produit oracle au-delà de l'écran de démarrage** — non déterminé.
- **Pourquoi `timeout 90` n'a pas terminé le processus au bout de 90 s
  réelles** — le processus a continué à tourner significativement plus
  longtemps ; non investigué (possible interaction avec `SIGTERM`
  bloqué par le processus, ou artefact de l'environnement d'exécution
  de cette tâche).

## Décisions prises

- **Ne pas modifier `run_gate.py`** pour découpler `--dump_shaders` de
  `--mission-render-summance`/route scellée ce cycle — le contournement
  par lancement direct a suffi pour tester l'hypothèse la moins
  coûteuse en premier, sans toucher à l'outil partagé.
- **Ne pas enchaîner immédiatement sur une nouvelle tentative avec
  entrée synthétique** (ex. une pression de touche après quelques
  secondes) — respect de la discipline de portée bornée : un cycle,
  une hypothèse testée, un résultat honnête, pas une exploration
  ouverte de séquences d'entrée.
- Nettoyage complet : processus `ac6recomp`/`Xvfb :190` arrêtés,
  répertoire de travail temporaire supprimé.

## Gate

**Aucune source de production modifiée ce cycle** — lancement direct
du binaire déjà compilé, aucune reconstruction, aucun fichier
`native/` touché.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
`ctest` natif et racine inchangés (aucune source de production
modifiée ce cycle ; profil `native` non touché, aucune vérification
supplémentaire nécessaire).

## Named for r480

Deux pistes concrètes, ni l'une ni l'autre décidée ce cycle : (1)
répéter le lancement direct avec une entrée synthétique minimale
(ex. une pression `A`/`Space` après quelques secondes, ou une
séquence courte de progression de menu) pour voir si le jeu de
nuanceurs cible apparaît après un premier écran de confirmation ; (2)
modifier `run_gate.py` pour découpler `--dump_shaders` du chemin de
route scellée, permettant une route diagnostique non scellée avec
capture de nuanceurs — un changement d'outil plus propre mais qui
touche un fichier partagé par toute la campagne oracle. Reste ouvert,
indépendant : Piste B (committer l'arriéré natif stabilisé r433-r478)
et Piste C (garde-fou HUD) du plan approuvé.

## Files

Aucun artefact gitignoré nouveau conservé (répertoire de travail
temporaire supprimé après extraction des résultats).

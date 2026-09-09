# AC6 retail NTSC-U/J — r481 — piste A (reprise, décision utilisateur explicite) : un nouveau drapeau `--mission-dump-shaders` découplé de D5B4 confirme, par une méthode indépendante, la même paire (nuanceur, modification) manquante déjà nommée par r477/r478 ; le troisième nuanceur cible reste introuvable ; une extension de route vers le sélecteur de sauvegarde échoue par un blocage « movie worker », non investiguée plus loin

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle utilisé (reprise explicite de la piste A par décision
utilisateur, après la pause recommandée par r480 et actée en fin de
plan `groovy-beaming-book.md`).

## Contexte

Nommé par r479, piste 2 (jamais tentée jusqu'ici) : découpler
`--dump_shaders` de `--mission-d5b4-final-white`/
`--mission-render-summary` dans `run_gate.py`, pour dumper des
nuanceurs sur une route diagnostique COURTE (`--diagnostic-route`,
`is_sealed_route()` non requis) plutôt que de continuer à bricoler
autour d'un lancement brut du binaire (approche de r479/r480, qui a
buté sur une divergence de cadencement de démarrage jamais expliquée,
700+s vs <30s).

**Contrainte non négociable de ce cycle, respectée intégralement** :
une session concurrente distincte et non identifiée modifie en ce
moment même, sans commit, `native/src/native_vulkan_backend.cpp`,
`native/include/ac6/native_xenos.h`, `native/src/native_xenos.cpp`,
`native/src/native_vulkan_device.cpp`,
`native/tests/native_xenos_tests.cpp` et
`native/fixtures/pinned-shader-registry.v1.bin` (chaîne de rapports
`reports/ac6-retail-native-r48*-*.md`, numérotation indépendante,
dernière écriture 2026-09-09 05:06:56). **Aucun fichier sous
`recompilation/ace-combat-6-retail/native/` n'a été touché ce
cycle** — vérifié par `git status --porcelain` avant commit (voir
Files) — uniquement lecture (le mapping `primitive_type`/digest déjà
établi par r476/r478 a été réutilisé, pas relu).

## Établi — nouveau drapeau `--mission-dump-shaders`, additif, vérifié sans toucher `native/`

`run_gate.py` gardait `--dump_shaders=<dir>` strictement derrière
`--mission-d5b4-final-white` (lui-même derrière
`--mission-render-summary`, qui exige les marqueurs de route complets
d'un cinématique de campagne). Ajout d'un drapeau indépendant :
`--mission-dump-shaders` (nécessite `--diagnostic-route`, erreur
explicite sinon), qui ajoute `--dump_shaders=<sortie>/shader-dump` à
la commande de lancement SANS activer aucun des drapeaux de
diagnostic D5B4/render-summary. Testé (`--help` affiche le nouveau
drapeau, validation d'argument vérifiée) et exécuté avec succès.

## Établi — capture réussie sur `routes/us-pretype28-startup.steps`, mais AUCUNE nouvelle information sur les 3 états cibles

`run_gate.py --diagnostic-route routes/us-pretype28-startup.steps
--mission-dump-shaders` : run complet, `game_status=0`,
`clean_shutdown=true`, `executed_steps=8/8`, 56.4 s réelles (bien
plus rapide que tout lancement direct du binaire de r479/r480 — ceci
confirme, sans l'expliquer davantage, que la lenteur de r479/r480
tenait au lancement brut hors `run_gate.py`, pas au produit
oracle-hybride lui-même). 11 nuanceurs / 8 pipelines / 12 paires
capturés par `parse_rexglue_cache.py` (aucune troncature — contraste
avec r471/r476/r477 qui devaient exclure des nuanceurs non persistés
par un arrêt abrupt ; ici l'arrêt est propre).

Digests FNV-1a64 recalculés directement (fonction `fnv1a64()` de
`materialize_pinned_shader_capsule.py`, importée à l'identique,
appliquée aux fichiers `.ucode.bin.{vert,frag}` du dump) :

```
shader_C049A8C9E556F129.ucode.bin.vert  → 09dd1c7cddae1141  (CIBLE)
shader_0A6D1DD7767FDF27.ucode.bin.vert  → 57b8e5f14b93cff4  (CIBLE)
```

Ces deux nuanceurs cibles SONT dans ce dump — mais avec les mêmes
modifications déjà rapportées par r477 (méthode indépendante :
`mission01-qualified-96.steps` tronqué à `--duration 20`, contre une
route diagnostique dédiée de bout en bout ici) :

```
c049a8c9e556f129 modification=0000000900000000  (opposé de la cible : 0x0)
0a6d1dd7767fdf27 modification=0000000000000001  (opposé de la cible : 0x900000000)
```

**Confirmation indépendante, par un mécanisme de capture entièrement
différent, du diagnostic déjà établi par r477/r478** : ce n'est ni un
artefact de fenêtre de capture (r477) ni un bug de décodage PM4
(r478) — la valeur de modification alternative n'apparaît tout
simplement dans AUCUNE route testée jusqu'ici, cette nouvelle
comprise. `4dd456c4ea0923c1` (troisième cible) reste absent de ce
dump également — aucun nuanceur capturé ne correspond à ce digest.

Nuanceur nouveau, non vu par les cycles précédents :
`shader_EA41C0069AE03769.ucode.bin.vert` → `17dbf9b1e3d6ae04`
(modification `0000000000000007`) — pas une des 3 cibles, mais une
couverture réelle supplémentaire, laissée dans le scratch
(`/fastdata/tmp/r481-scratch/`, non conservé, voir Files) plutôt que
mergée (contrainte de ce cycle : aucun merge dans `native/`).

## Non établi — extension de route vers le sélecteur de sauvegarde : incomplète, cause non déterminée

Hypothèse motivée : un curseur de sélection en surbrillance dans un
écran de menu (sélecteur de sauvegarde `game-data-browser`,
`selector44=3`, atteint par les lignes 6-13 de
`mission01-qualified-96.steps`) pourrait dessiner avec un
`primitive_type` différent de l'écran-titre statique. Route
diagnostique dédiée créée
(`routes/us-menu-navigation-probe.steps`, mêmes touches/attentes
exactes que les 13 premières lignes de la route scellée) et lancée
deux fois. **Les deux lancements n'ont jamais atteint `type28=30`**
(0 occurrence de `type28`/`selector44` dans `ac6recomp.log`) — le
journal montre une boucle `KeWaitForMultipleObjects`/`KeSetEvent`
continue sur le « AC6 movie worker », suggérant un blocage sur une
attente de cinématique/vidéo d'intro non présente (ou différemment
minutée) sur cette route par rapport à `us-pretype28-startup.steps`
qui, lui, réussit de façon reproductible. **Cause non déterminée** —
hors périmètre d'investigation de ce cycle (aucune preuve de lecture
directe du code du « movie worker », pas d'affirmation risquée).
Processus arrêtés manuellement (`kill -9`) après expiration du délai
de 180 s ; aucune fuite de processus résiduelle vérifiée après coup
(`pgrep` propre).

## Décisions prises

- Ne PAS toucher `native/` ce cycle, conformément à la contrainte
  explicite de coordination avec la session concurrente non
  identifiée — même en cas de succès de capture, le merge du registre
  aurait été reporté à un cycle ultérieur, une fois l'arbre `native/`
  confirmé calme.
- Ne pas poursuivre le débogage du blocage « movie worker » de
  `us-menu-navigation-probe.steps` ce cycle — deux tentatives
  infructueuses, cause non lue dans le code, rendement décroissant
  déjà signalé comme motif de pause par r480 pour un symptôme
  similaire (divergence de cadencement).
- Conserver `routes/us-menu-navigation-probe.steps` (fichier créé, non
  fonctionnel en l'état) et le drapeau `--mission-dump-shaders`
  (fonctionnel, vérifié) — les deux sont des outils réutilisables,
  pas des artefacts de scratch.

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts (`JF=pass`, `contract_artifacts=pass`,
`contract_addresses=pass`, inchangés — aucune source `native/`
touchée). `ctest` natif délibérément NON relancé ce cycle (aucun
fichier natif modifié, et la session concurrente a l'arbre `native/`
mid-edit — relancer `ctest` natif dessus produirait un résultat qui
n'appartient à aucune des deux sessions). `git status --porcelain`
après ce travail confirme zéro chemin sous
`recompilation/ace-combat-6-retail/native/` modifié par ce cycle.

## Named for r482

**Piste A reste ouverte, sans nouvel élément permettant un merge** :
les 3 états cibles de r478 restent non capturés sous la bonne
modification (2/3) ou pas du tout (1/3), maintenant confirmé par DEUX
méthodes de capture indépendantes (r477 : route scellée tronquée ;
r481 : route diagnostique dédiée). Candidats pour la suite,
non tentés :
1. Déboguer le blocage « movie worker » de
   `us-menu-navigation-probe.steps` (lecture directe du code du movie
   worker avant toute nouvelle tentative de route, pas une nouvelle
   supposition de minutage).
2. Une fois l'arbre `native/` de la session concurrente confirmé
   calme (commité ou en pause stable), fusionner les 12 traductions
   de ce cycle (aucune des 3 cibles, mais 1 nuanceur réellement
   nouveau) dans le registre — bénéfice marginal, pas prioritaire tant
   que les cibles elles-mêmes manquent.
3. Considérer que ces 3 états pourraient n'être atteignables que par
   la route scellée complète (hangar/déploiement), jamais testée avec
   `--mission-dump-shaders` faute de temps ce cycle — mais cela
   dépenserait à nouveau un budget oracle substantiel (573s+, r470)
   pour un résultat incertain.

## Files

Committé : `recompilation/ace-combat-6-retail/tools/run_gate.py`
(nouveau drapeau `--mission-dump-shaders`),
`recompilation/ace-combat-6-retail/routes/us-menu-navigation-probe.steps`
(route diagnostique, non fonctionnelle en l'état — blocage non
résolu), ce rapport, `NEXT.md`, `reports/handoff/CURRENT.json`.
Non conservé (scratch, `/fastdata/tmp/r481-scratch/`) : dumps de
nuanceurs bruts, manifestes de traduction, deux répertoires de sortie
`run_gate.py`. Aucun fichier sous
`recompilation/ace-combat-6-retail/native/` modifié.

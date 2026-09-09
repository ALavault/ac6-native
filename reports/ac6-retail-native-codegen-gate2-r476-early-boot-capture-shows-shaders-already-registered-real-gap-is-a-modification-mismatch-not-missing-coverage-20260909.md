# AC6 retail NTSC-U/J — r476 — piste A (plan approuvé) : une capture oracle ciblée tôt-démarrage produit 10 traductions, TOUTES déjà dans le registre à 320 entrées ; le vrai trou n'est pas une couverture de nuanceur manquante mais une PAIRE (nuanceur, modification) manquante pour les tirages réels du sondage natif

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle utilisé (piste A du plan approuvé — décision utilisateur déjà
actée).

## Contexte

Piste A du plan approuvé (`/home/lavaulta/.claude/plans/groovy-beaming-book.md`),
nommée par r475 : une campagne oracle ciblée sur le contenu de tout
début (logo/écran-titre), pas la route de vol de r470, pour capturer
les nuanceurs que le sondage natif atteint réellement.

## Établi — capture réussie, chaîne complète rejouée, mais ZÉRO nuanceur réellement nouveau

`run_gate.py` refuse toute route personnalisée non scellée avec
`--mission-render-summary`/`--mission-d5b4-final-white`
(`is_sealed_route()` — seules les trois routes pré-enregistrées sont
acceptées). Solution : réutiliser la route scellée
`routes/mission01-qualified-96.steps` mais avec `--duration 60`,
l'interrompant délibérément tôt (bien avant la création de sauvegarde)
pour ne capturer que le contenu de tout début.

Chaîne rejouée intégralement (mêmes outils que r471/r472, aucune
modification d'outillage) :
1. `run_gate.py --route routes/mission01-qualified-96.steps
   --mission-render-summary --mission-d5b4-final-white --duration 60` →
   `game_status=-9` (« route duration exceeded », attendu, provoqué
   délibérément), mais `cache/shaders/shareable/4E4D07D1.{xsh,fsi.vk.xpso}`
   produits avec du contenu réel.
2. `parse_rexglue_cache.py` : 2 nuanceurs dumpés jamais persistés dans
   le `.xsh` (arrêt abrupt, même symptôme que r471) — exclus du
   répertoire de dump avant parsing (resserre la validation, ne la
   contourne pas, même méthode que r471). Résultat : **9 nuanceurs, 7
   pipelines, 10 paires de traduction**.
3. `rexglue_shader_translate` (binaire reconstruit) : **10/10
   traductions réussies**, mêmes cvars de correctifs AC6 que
   r254/r471.
4. Bundle construit au format `ac6.native-us-ucode-frontier.v1`
   (script ad hoc réutilisant `fnv1a64()` importé directement de
   `materialize_pinned_shader_capsule.py`, même pattern que r471) ;
   `build_capsule()` (importé, pas réimplémenté) l'a validé sans
   erreur — capsule de 10 entrées.
5. **Fusion contre le registre actuel (320 entrées, dédup par
   `(shader_type, digest, modification, spirv)`, même clé que r471) :
   `merged entries: 320`, `genuinely new: 0`.** Les 10 traductions de
   ce cycle correspondent EXACTEMENT (même digest, même modification,
   même SPIR-V) à des entrées déjà présentes dans le registre à 320
   entrées issu de la route de vol de r470/r472. Cohérent : la route
   de vol commence elle-même par le même écran-titre avant d'atteindre
   le vol, donc son propre dump a déjà couvert ce contenu.

## Établi — le vrai trou, via une trace en direct : une PAIRE (nuanceur, modification) manquante, pas un nuanceur manquant

Ce résultat contredit la piste supposée par r475/r476 (couverture de
nuanceur manquante). Diagnostic ajouté (local, gardé par
`AC6_NATIVE_VD_TRACE`, non committé) juste avant le message d'erreur
« active shader is not pinned » dans `draw_pinned()`, imprimant les
digests de nuanceurs et modifications réellement essayées. Sondage
natif relancé (`ctest` 11/11 avant/après, aucune régression) :
```
r476 probe: vertex_digest=09dd1c7cddae1141 vertex_mod=0000000000000000 vertex_ok=0 pixel_ok=1
r476 probe: vertex_digest=4dd456c4ea0923c1 vertex_mod=0000000900000000 vertex_ok=0 pixel_ok=1
r476 probe: vertex_digest=57b8e5f14b93cff4 vertex_mod=0000000900000000 vertex_ok=0 pixel_ok=1
```
**Le nuanceur pixel correspond systématiquement (`pixel_ok=1`) — le
rejet vient TOUJOURS du côté vertex.** Et deux des trois digests
vertex rejetés sont EXACTEMENT ceux capturés à l'étape précédente de
ce même cycle :
- `09dd1c7cddae1141` = `shader_C049A8C9E556F129.ucode.bin.vert`,
  capturé avec `modification=0x900000000` — mais le tirage natif réel
  demande `modification=0x0` (aucun candidat rect-list).
- `57b8e5f14b93cff4` = `shader_0A6D1DD7767FDF27.ucode.bin.vert`,
  capturé avec `modification=0x1` — mais le tirage natif réel demande
  `modification=0x900000000` (rect-list).

**Le nuanceur EST dans le registre — juste pas sous la modification
que ce tirage précis demande.** `vertex_high` ne prend que deux
valeurs (`0` ou `9<<32`, selon `rect_strip_expand`) ; la capture
oracle de ce cycle a rencontré ces mêmes nuanceurs vertex dans un
contexte de primitive différent (probablement un écran de chargement/
diagnostic — `RESULT.json` de l'étape 1 montrait
`step-02-failure-observation.png`, pas l'écran-titre final), donc sous
la MAUVAISE valeur de `vertex_high` pour les tirages que le sondage
natif atteint réellement.

## Non établi

- **Quel écran précis** le sondage natif atteint réellement (toujours
  non identifié avec certitude — seule la forme des tirages est
  connue, r475).
- **Si une capture plus longue ou différemment chronométrée** (viser
  spécifiquement le premier écran stable après le tout premier
  chargement, pas la fenêtre 0-60s qui inclut apparemment un écran de
  diagnostic/erreur transitoire) capturerait ces mêmes nuanceurs vertex
  sous la BONNE modification — plausible, non testé ce cycle (budget
  du cycle déjà consommé par la chaîne complète ci-dessus).

## Décisions prises

- Ajouter le diagnostic de trace (digests + modifications essayées)
  plutôt que de deviner la cause du rejet — corrige une prémisse
  fausse (couverture manquante) avant qu'un mauvais correctif ne soit
  tenté.
- Ne PAS relancer une nouvelle capture oracle ce cycle pour cibler
  précisément le bon écran — le diagnostic ci-dessus donne déjà une
  piste concrète et peu coûteuse (ajuster le minutage/la durée de
  capture) pour le prochain cycle, plutôt que de deviner à l'aveugle.
- Toujours ne pas committer `native_vulkan_backend.cpp` (diagnostic de
  trace inclus) — même entremêlement que l'arriéré déjà catalogué.
- Retirer le fichier de route personnalisé initialement créé
  (`routes/r476-earliest-boot-diagnostic.steps`) — jamais utilisé en
  pratique (`run_gate.py` refuse les routes non scellées avec les
  drapeaux de dump), la route scellée avec `--duration` tronqué a
  suffi.

## Gate

**Aucune source de production committée ce cycle** — le diagnostic de
trace reste appliqué localement, vérifié (build + `ctest` 11/11 +
lancement réel avec trace), non committé.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées — aucune source de production committée ce cycle)
`ctest` racine inchangé (aucune source de production committée ce
cycle).

## Named for r477

Relancer une capture oracle ciblée avec un minutage différent — viser
spécifiquement au-delà de l'écran de diagnostic/erreur transitoire
observé à l'étape 1 de ce cycle (`step-02-failure-observation.png`),
pour capturer les MÊMES nuanceurs vertex déjà identifiés
(`09dd1c7cddae1141`, `57b8e5f14b93cff4`, et `4dd456c4ea0923c1` non
encore localisé dans le dump de ce cycle) sous la modification que
les tirages réels du sondage natif demandent. C'est maintenant une
cible précise et peu coûteuse (2-3 nuanceurs vertex, pas une nouvelle
campagne large), pas une exploration à l'aveugle. Reste ouvert sinon :
Piste B (committer l'arriéré natif) et Piste C (garde-fou HUD) du
plan approuvé, indépendantes de cette piste.

## Files

Aucun artefact gitignoré nouveau conservé (bundle/capsule/manifeste de
traduction sous `/fastdata/lavaulta/tmp/`, non conservés).

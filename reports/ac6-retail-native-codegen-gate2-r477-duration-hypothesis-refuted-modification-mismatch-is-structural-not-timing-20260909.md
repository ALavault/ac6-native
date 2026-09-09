# AC6 retail NTSC-U/J — r477 — piste A (plan approuvé) : l'hypothèse de minutage de r476 est RÉFUTÉE — la paire (nuanceur, modification) manquante est structurelle, pas un artefact de fenêtre de capture

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle utilisé (piste A du plan approuvé — décision utilisateur déjà
actée).

## Contexte

Nommé par r476 : relancer une capture oracle ciblée avec un minutage
différent, pour capturer les deux nuanceurs vertex déjà identifiés
(`09dd1c1cddae1141`/`shader_C049A8C9E556F129`,
`57b8e5f14b93cff4`/`shader_0A6D1DD7767FDF27`) sous la modification que
les tirages réels du sondage natif demandent, au lieu de celle
rencontrée par la capture initiale (attribuée à un écran de
diagnostic/erreur transitoire à `--duration 60`).

## Établi — deux routes scellées « longstart » sont inutilisables avec les drapeaux de dump

`routes/mission01-qualified-96-longstart-v2-candidate.steps` et
`routes/mission01-qualified-96-longstart-candidate.steps` (les deux
autres routes scellées reconnues par `is_sealed_route()`) commencent
par `sleep 60` sans aucune entrée synthétique — a priori idéal pour
capturer l'écran statique de tout début, exactement comme le sondage
natif zéro-entrée. Mais `run_gate.py` exige les marqueurs de capture
`campaign-new-game` ET `post-campaign-intro-skip` dès que
`--mission-render-summary`/`--mission-cinematic-handoff` est actif
(ligne ~805-822) — la route « longstart-v2-candidate » ne porte PAS
de marqueur `campaign-new-game` (restructurée différemment, sans ce
point d'arrêt) et « longstart-candidate » ne porte AUCUN marqueur
`capture` du tout. **Erreur immédiate** : « cinematic handoff campaign
markers missing ». Seule `routes/mission01-qualified-96.steps` (déjà
utilisée par r476, celle qui envoie `Escape` dès `t=0.1s`) porte les
deux marqueurs requis — c'est la SEULE route scellée utilisable avec
la chaîne de dump de nuanceurs actuelle.

## Établi — l'hypothèse de minutage est réfutée par une preuve directe

Relancé `run_gate.py --route mission01-qualified-96.steps
--mission-render-summary --mission-d5b4-final-white --duration 20`
(au lieu de 60) — même route que r476, durée réduite. Résultat :
`step-01-failure-observation.png` (au lieu de `step-02-...` à
`--duration 60`) — la capture d'échec de `run_gate.py` apparaît à
CHAQUE durée testée, avec un numéro qui suit simplement l'étape de
route atteinte au moment de la coupure, pas un état de jeu
particulier. **Ce n'est probablement qu'un nom de capture générique du
côté outillage** (« la route ne s'est pas terminée proprement »), pas
la preuve d'un écran de diagnostic/erreur en jeu que r476 avait
supposée.

Rejoué la même chaîne de traitement que r471/r476 (deux nuanceurs non
persistés dans le `.xsh` exclus du dump avant analyse, même méthode
resserrant la validation plutôt que la contournant — cette fois
`shader_472913F460D4B446.ucode.bin.vert` et
`shader_8F1C48BA92C8E43E.ucode.bin.frag`, aucun des deux nuanceurs
ciblés) :
```
0a6d1dd7767fdf27 (0A6D1DD7767FDF27) modification=0000000000000001
c049a8c9e556f129 (C049A8C9E556F129) modification=0000000900000000
```
**IDENTIQUE au résultat de r476 à `--duration 60`** — les deux
nuanceurs apparaissent avec exactement les mêmes valeurs de
modification (donc le même appariement primitive_type/rect-list),
malgré une fenêtre de capture réduite de deux tiers et un point
d'arrêt de route complètement différent. **Ceci réfute directement
l'hypothèse « mauvais minutage/écran transitoire »** : la route
scellée `mission01-qualified-96.steps` associe TOUJOURS ces deux
nuanceurs à ces modifications précises, quel que soit l'instant précis
de la coupure.

## Établi — la nature du désaccord est structurelle : `rect_strip_expand` ne dépend que de `primitive_type`

Relecture de `draw_pinned()` (`native/src/native_vulkan_backend.cpp`,
ligne ~2685) : `vertex_high = rect_strip_expand ? (9<<32) : 0`, où
`rect_strip_expand = (draw.primitive_type == 0x08u)` — une valeur
directement décodée du flux PM4 invité par `native_xenos.cpp`, sans
ambiguïté ni logique dérivée à corriger côté natif. Le désaccord
observé n'est donc PAS une question de logique de résolution de
modification (contrairement au bug d'ordre de candidats corrigé en
r472, qui portait sur le nuanceur PIXEL, pas vertex) — c'est un
désaccord sur la valeur RÉELLE de `primitive_type` associée à ces deux
nuanceurs entre (a) le contexte d'usage que la route oracle scellée
traverse, et (b) le contexte d'usage que le sondage natif zéro-entrée
atteint réellement. Ce sont potentiellement deux ÉCRANS DIFFÉRENTS de
l'interface qui réutilisent les mêmes programmes de nuanceurs vertex
avec des types de primitive différents (un motif plausible pour des
nuanceurs UI génériques réutilisés) — pas nécessairement un bug de
décodage PM4 natif.

## Non établi

- **Quel écran précis le sondage natif zéro-entrée atteint réellement**
  — toujours non identifié (r475/r476 laissaient déjà ceci ouvert).
- **Si un décodage PM4 natif (`native_xenos.cpp`) diverge du
  comportement matériel réel pour ces deux tirages précis** — non
  vérifié directement (nécessiterait de comparer le flux PM4 brut du
  sondage natif à ce que l'oracle produirait pour le MÊME état de jeu,
  ce que les trois routes scellées disponibles ne permettent pas
  d'atteindre).
- **Si un point d'arrêt DIFFÉRENT à l'intérieur de la fenêtre
  0-20s** (essayé), ou une valeur intermédiaire jamais testée,
  produirait un résultat différent — peu probable étant donné
  l'identité exacte du résultat entre 20s et 60s, mais pas
  formellement exclu pour toute la plage possible.

## Décisions prises

- **Ne PAS tester d'autres valeurs de `--duration`** — la preuve à
  20s contre 60s (deux points bien séparés dans la plage utile,
  résultat identique) suffit à réfuter l'hypothèse de minutage sans
  épuiser davantage le cycle sur une recherche par force brute.
- **Ne PAS modifier `native_xenos.cpp`** pour deviner un correctif de
  décodage `primitive_type` sans preuve directe (flux PM4 brut du
  sondage natif jamais comparé à une trace oracle du MÊME écran) —
  exactement le type de correctif non vérifié que la discipline de ce
  dépôt refuse.
- **Ne PAS tenter de contourner le scellement des routes** (éditer une
  route casserait son hash, `is_sealed_route()` la rejetterait) — un
  contournement, pas une piste légitime.
- Nettoyage complet des artefacts temporaires (`/fastdata/lavaulta/tmp/
  ac6-r477-earlyboot/`, non conservés).
- Toujours ne pas committer `native_vulkan_backend.cpp` (diagnostic de
  trace de r476 toujours présent localement) — même entremêlement que
  l'arriéré déjà catalogué.

## Gate

**Aucune source de production modifiée ce cycle** — investigation par
lancements/analyse uniquement.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
`ctest` natif : 11/11 (inchangé, aucune source touchée). `ctest`
racine inchangé.

## Named for r478

Deux pistes distinctes, ni l'une ni l'autre à trancher seule :

1. **Décoder le flux PM4 brut du sondage natif** pour ces deux tirages
   précis (via `AC6_NATIVE_VD_TRACE`, imprimer `draw.primitive_type`
   directement, pas seulement le `vertex_high` dérivé) — établirait
   sans ambiguïté si le désaccord vient d'un vrai bug de décodage
   `native_xenos.cpp` (auquel cas un correctif est possible sans
   oracle) ou d'un état de jeu génuinement différent (auquel cas seule
   une capture oracle du MÊME écran résoudrait le problème, mais
   aucune des trois routes scellées disponibles n'y donne accès —
   dépenser du budget oracle pour ENREGISTRER une nouvelle route
   scellée capable d'atteindre cet écran précis serait une décision à
   ne pas prendre seul, étant donné le budget déjà dépensé sur cette
   campagne).
2. Reste ouvert, indépendant : Piste B (committer l'arriéré natif
   stabilisé r433-r477) et Piste C (garde-fou HUD) du plan approuvé.

## Files

Aucun artefact gitignoré nouveau conservé (dumps/parses/bundles sous
`/fastdata/lavaulta/tmp/ac6-r477-earlyboot/`, supprimés).

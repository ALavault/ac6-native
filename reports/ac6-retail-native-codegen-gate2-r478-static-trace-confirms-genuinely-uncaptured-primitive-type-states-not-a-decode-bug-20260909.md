# AC6 retail NTSC-U/J — r478 — piste A (plan approuvé) : trace statique confirme que les rejets restants sont trois états `primitive_type` réellement non capturés, PAS un bug de décodage PM4 natif

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé (décision utilisateur explicite : trace statique
d'abord, décision de nouvelle capture oracle seulement après).

## Contexte

Nommé par r477, piste 1 : décoder `draw.primitive_type` directement
(pas seulement `vertex_high` dérivé) via `AC6_NATIVE_VD_TRACE`, pour
trancher entre un vrai bug de décodage PM4 natif (`native_xenos.cpp`)
et un état de jeu génuinement différent de ceux que les routes oracle
scellées disponibles atteignent.

## Établi — trace directe, comparée au registre réel

Ajout d'un champ `primitive_type`/`rect_strip_expand` à la trace de
diagnostic existante de r476 (`native/src/native_vulkan_backend.cpp`,
non committée, même statut que l'arriéré déjà catalogué). Relancé
(`AC6_NATIVE_VD_TRACE=1 AC6_NATIVE_CAPTURE=1
AC6_NATIVE_PROBE_WINDOW_MS=25000`, ISO réelle) :
```
primitive_type=0x01 vertex_digest=09dd1c7cddae1141 vertex_mod=0000000000000000
primitive_type=0x08 vertex_digest=4dd456c4ea0923c1 vertex_mod=0000000900000000
primitive_type=0x08 vertex_digest=57b8e5f14b93cff4 vertex_mod=0000000900000000
```
(le nuanceur pixel `e41b4b062083e5bf` est IDENTIQUE et réussit
(`pixel_ok=1`) dans les trois cas — le blocage est uniquement côté
vertex, confirmant le cadrage de r477.)

Interrogation directe du registre à 320 entrées
(`native/fixtures/pinned-shader-registry.v1.bin`, via
`materialize_pinned_shader_capsule.parse_capsule`) pour ces trois
digests exacts :
```
09dd1c7cddae1141 : registre porte mod=0x900000000 ET mod=0x900000001
                    (rect_strip_expand=VRAI dans les deux cas)
                    — le sondage natif demande mod=0x0 (FAUX) : MANQUANT
57b8e5f14b93cff4 : registre porte mod=0x0 ET mod=0x1
                    (rect_strip_expand=FAUX dans les deux cas)
                    — le sondage natif demande mod=0x900000000 (VRAI) : MANQUANT
4dd456c4ea0923c1 : ABSENT du registre sous QUELQUE modification que ce soit
```
**Motif symétrique net** : les deux premiers nuanceurs sont chacun
déjà capturés, mais uniquement pour la valeur OPPOSÉE de
`rect_strip_expand` que le sondage natif demande réellement — pas un
« quasi-manque », une inversion complète et cohérente pour les deux.
Le troisième est un nuanceur entièrement nouveau, jamais rencontré par
aucune capture précédente (r254/r255/r470/r471/r476).

## Établi — ce n'est pas un bug de décodage PM4 natif

Lecture de `native_xenos.cpp` (lignes ~350, ~364) : `primitive_type`
est extrait par un simple masque `payload[N] & 0x3Fu` (6 bits du
registre `VGT_DRAW_INITIATOR`, l'encodage Xenos standard), appliqué
**uniformément à chaque tirage**, sans aucune dépendance au nuanceur
lié ou à un état persistant entre tirages. Il n'existe **aucun
mécanisme plausible** par lequel ce décodage pourrait produire une
« inversion » spécifique à ces deux nuanceurs précis — le motif observé
(deux nuanceurs, primitive_type opposé à leur capture antérieure) est
exactement ce qu'on attend de deux nuanceurs vertex génériques
d'interface réutilisés par le jeu dans des contextes de dessin
différents (un tracé rectangle-liste quelque part, un tracé normal
ailleurs), pas d'un défaut de traduction PPC→hôte.

## Non établi

- **L'écran exact** que le sondage natif zéro-entrée atteint
  réellement (logo, écran-titre, autre) — toujours non identifié
  précisément, comme depuis r475.
- **Une route capable d'atteindre cet écran précis** — les trois
  routes scellées disponibles (`mission01-qualified-96.steps` et les
  deux variantes « longstart ») ne le permettent pas, la première par
  contenu (r477), les deux autres par incompatibilité d'outillage
  (marqueurs de capture manquants, r477).

## Décisions prises

- **Ne PAS lancer de nouvelle capture oracle ce cycle** — limite
  explicite de l'utilisateur pour ce cycle (trace statique d'abord),
  respectée.
- **Ne PAS modifier `native_xenos.cpp`** — aucune preuve d'un bug de
  décodage n'a été trouvée ; modifier ce fichier sans preuve directe
  serait exactement le type de correctif non vérifié que ce dépôt
  refuse.
- La trace de diagnostic ajoutée reste locale, non committée — même
  statut que l'arriéré déjà catalogué (`native_vulkan_backend.cpp`).

## Gate

**Aucune source de production committée ce cycle** — trace de
diagnostic locale uniquement, vérifiée (build + `ctest` 11/11 +
lancement réel avec trace), non committée.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées — aucune source de production committée ce cycle)
`ctest` racine inchangé (aucune source de production committée ce
cycle) ; `ctest` natif 11/11.

## Named for r479

**Décision utilisateur requise avant de reprendre** : les trois
motifs manquants (deux inversions de `primitive_type`, un nuanceur
entièrement nouveau) nécessitent une NOUVELLE route oracle scellée
capable d'atteindre l'écran précis que le sondage natif zéro-entrée
traverse — aucune des trois routes scellées existantes n'y donne
accès. Enregistrer une nouvelle route (probablement une capture très
courte, sans entrée du tout, similaire au comportement du sondage
natif lui-même) est un travail réel qui dépense davantage le budget
oracle déjà largement utilisé par cette campagne (r454-r478) — à ne
pas décider seul. Reste ouvert, indépendant : Piste B (committer
l'arriéré natif stabilisé r433-r478) et Piste C (garde-fou HUD) du
plan approuvé.

## Files

Aucun artefact gitignoré nouveau conservé.

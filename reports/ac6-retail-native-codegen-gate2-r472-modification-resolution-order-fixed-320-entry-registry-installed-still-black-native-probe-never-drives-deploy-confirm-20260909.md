# AC6 retail NTSC-U/J — r472 — la sémantique de résolution multi-modification laissée ouverte par r471 est COMPRISE et corrigée (ordre de repli inversé par rapport à l'intention documentée) ; le registre fusionné à 320 entrées est installé, vérifié, sans régression — mais la sonde native elle-même ne traverse jamais la séquence de confirmation de déploiement, donc `non_black` reste à 0 ce cycle

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun nouveau lancement oracle ce cycle — travail entièrement statique
sur les artefacts déjà produits par r471, toujours présents sur disque
sous `/fastdata/lavaulta/tmp/ac6-r471-fsi-parse/`.

## Contexte

Nommé par r471 : comprendre précisément la sémantique de résolution
de `translate_ucode_variant()` face à plusieurs variantes candidates
pour un même digest, avant tout merge du registre fusionné à 320
entrées (271 existantes + 49 nouvelles, issues du dump `world=1` de
r470).

## Établi — la cause précise, par lecture directe du code (pas une supposition)

`draw_pinned()` (`native/src/native_vulkan_backend.cpp`) essaie quatre
candidats de modification dans l'ordre, s'arrêtant au premier qui
correspond réellement à une entrée épinglée dans
`translate_ucode_variant()` :
```
pixel_candidates[4] = {
    pixel_high,                          // [0] indice de profondeur exact, sans ParamGen
    pixel_no_depth,                      // [1] repli : indice de profondeur abandonné
    pixel_no_depth | kParamGenEnable,    // [2] repli : indice abandonné + ParamGen
    pixel_high | kParamGenEnable,        // [3] indice de profondeur exact + ParamGen
};
```
Le commentaire au-dessus déclare l'intention : « the pixel early-Z
hint falls back to kNoModifiers when no variant carries the hint » —
c'est-à-dire que l'abandon de l'indice de profondeur doit être un
DERNIER recours. Mais l'ORDRE réel place les deux candidats « indice
abandonné » (indices 1 et 2) AVANT le candidat « indice exact +
ParamGen » (indice 3) — l'inverse de l'intention déclarée.

Inspection directe du registre fusionné
(`/fastdata/lavaulta/tmp/ac6-r471-fsi-parse/merged-registry.v1.bin`,
via `materialize_pinned_shader_capsule.py::parse_capsule`) pour le
digest du test en échec
(`0xb10dc382bef32209`, shader pixel) : **deux entrées**,
`modification=0x410000000000` (originale, bit46|bit40 = indice de
profondeur ET ParamGen) et `modification=0x10000000000` (nouvelle,
bit40 seul = ParamGen SANS l'indice de profondeur). Avec le registre à
271 entrées, seule la première existait ; la boucle atteignait le
candidat [3] (seul à correspondre) et retournait la bonne variante.
Avec le registre à 320 entrées, le candidat [2] (`pixel_no_depth |
kParamGenEnable` = 0x10000000000) trouve maintenant une correspondance
réelle et la boucle s'arrête là, **avant même d'essayer le candidat
[3]** qui aurait donné la variante respectant l'état réel du registre
(RB_COLORCONTROL=0 → indice de profondeur actif). Ce n'est pas une
ambiguïté nécessitant une connaissance oracle — c'est un bug d'ordre
de repli directement contredit par le commentaire du code lui-même.

## Correctif appliqué et vérifié (local, non committé pour le code source — voir ci-dessous pour ce qui EST committé)

Réordonné : les deux candidats à indice de profondeur EXACT (sans puis
avec ParamGen) sont essayés AVANT les deux candidats à indice
ABANDONNÉ, conformément à l'intention déjà documentée :
```
pixel_candidates[4] = {
    pixel_high,                          // exact, sans ParamGen
    pixel_high | kParamGenEnable,        // exact, avec ParamGen
    pixel_no_depth,                      // repli : indice abandonné
    pixel_no_depth | kParamGenEnable,    // repli : indice abandonné, ParamGen
};
```

## Établi — le registre à 320 entrées est installé et vérifié sans régression

- Registre fusionné de r471 copié dans
  `native/fixtures/pinned-shader-registry.v1.bin` (320 entrées, 255
  signatures de fetch distinctes, 65 avec plus d'une modification —
  recalculé directement par lecture du fichier, pas une supposition).
- `native/tests/native_xenos_tests.cpp` : les assertions de comptage
  exact (`entries.size() == 271u`, `vertex_count == 125u`,
  `pixel_count == 146u`) mises à jour vers les vraies valeurs (320,
  127, 193 — lues directement, pas devinées).
- Un second test cassé par le merge, non anticipé par r471 :
  `bundled_pinned_registry_fully_hits` échouait aussi sur
  `hit.spirv == first_spirv.at(...)` pour le chemin LEGACY
  (`ShaderTranslator::translate_ucode()`, sans argument de
  modification — distinct de `translate_ucode_variant()` utilisé par
  `draw_pinned()`). Cause : `translate_ucode()` essaie d'abord une
  correspondance EXACTE `modification == 0`, et ne retombe sur
  « première entrée enregistrée » que si aucune n'existe — le merge a
  introduit un cas où une entrée `modification == 0` NOUVELLE existe
  pour un digest dont l'entrée ORIGINALE (première dans l'ordre
  d'enregistrement) a une modification non nulle. La carte de
  vérification du test (`first_spirv`, construite par simple ordre
  d'insertion) ne reflétait pas cette règle à deux niveaux. Corrigé en
  construisant aussi `zero_mod_spirv` et en reproduisant exactement la
  règle réelle du code.
- `ctest` natif complet : **11/11**, y compris les deux tests
  précédemment cassés par le merge.

## Établi — le but final de cette campagne (contenu visible) N'EST PAS encore atteint, et pourquoi

Relancé la sonde `--probe-entry` habituelle
(`AC6_NATIVE_ALLOW_ENTRY_PROBE=1 AC6_NATIVE_VD_TRACE=1
AC6_NATIVE_CAPTURE=1`, ISO réelle) : **inchangé** —
`presented_frames=5 state=2`, `capture non_black=0 distinct_colors=1`,
8 replis « no pinned variant matches this draw state ». **Ce n'est PAS
une preuve que le registre étendu ne sert à rien** : la sonde
`--probe-entry` du produit `native` est une entrée COURTE (~25 s,
inchangée depuis r399) qui ne traverse jamais la longue séquence de
confirmation de déploiement (`--mission-hangar-confirm
--mission-map-confirm` + le quatrième A que r470 a ajouté) que
l'oracle a dû exécuter pendant ~10 minutes pour atteindre `world=1` et
capturer les nouveaux nuanceurs. Le produit `native` n'a jamais reçu
cette séquence d'entrée — son propre harnais de sonde ne sait
aujourd'hui driver que l'entrée directe en mission, pas la séquence de
campagne complète. Le registre étendu est réel et correctement
installé, mais **rien dans le run actuel de la sonde native atteint
les états de tirage que les 49 nouvelles entrées couvrent**.

## Non établi

- **Si la sonde native, drivée à travers la même séquence de
  confirmation que r470/r471 ont utilisée pour l'oracle, atteindrait
  enfin un contenu non noir** — non testé ce cycle (nécessiterait
  d'ajouter cette séquence d'entrée au harnais `--probe-entry` du
  produit natif, ou un mécanisme de rejeu d'entrée équivalent — un
  travail distinct, pas fait ce cycle).

## Décisions prises

- Committer UNIQUEMENT le fichier de données `native/fixtures/
  pinned-shader-registry.v1.bin` — contrairement à
  `native_vulkan_backend.cpp` et `native_xenos_tests.cpp`, ce fichier
  n'était PAS suivi par git avant ce cycle (`??`, jamais committé
  malgré la négation `.gitignore` dédiée depuis r255) : l'ajouter est
  un ajout propre et délibéré, pas un committage groupé de l'arriéré.
- Ne PAS committer `native_vulkan_backend.cpp` (correctif d'ordre des
  candidats) ni `native_xenos_tests.cpp` (assertions mises à jour) —
  ces fichiers portent l'arriéré massif déjà catalogué (r433 et
  suivants) ; les committer entraînerait de facto le committage de
  TOUT l'arriéré accumulé de ces fichiers, une décision hors du
  périmètre de ce cycle. Le correctif est documenté ici avec assez de
  détail pour être reproduit exactement.
- Ne pas tenter d'étendre le harnais de sonde native ce cycle — un
  travail réel distinct (rejeu d'entrée à travers la séquence de
  confirmation), pas une correction ponctuelle.

## Gate

Fichier de données committé, source non committée. `ctest` natif :
**11/11**.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
`ctest` racine : `ac6-cpp-complexity` seul échec pré-existant.

## Named for r473

Étendre le harnais `--probe-entry` du produit `native` (ou construire
un mécanisme de rejeu d'entrée équivalent) pour driver la même
séquence de confirmation de déploiement que r470/r471 ont utilisée
côté oracle, puis relancer la sonde pour vérifier si le registre à 320
entrées produit enfin du contenu visible non noir — le but final de
toute la chaîne r454-r472. Reste ouvert, non bloquant : le committage
groupé de l'arriéré `PinnedShaderRuntime` (r433/r434/r438/r454-r472,
incluant maintenant le correctif d'ordre des candidats et les
assertions de test mises à jour) — une décision séparée, une fois le
câblage jugé mûr.

## Files

`native/fixtures/pinned-shader-registry.v1.bin` (320 entrées, ajouté
et committé ce cycle). Artefacts intermédiaires de r471 toujours sous
`/fastdata/lavaulta/tmp/ac6-r471-fsi-parse/` (non conservés par
convention de ce dépôt, mais utilisés directement ce cycle).

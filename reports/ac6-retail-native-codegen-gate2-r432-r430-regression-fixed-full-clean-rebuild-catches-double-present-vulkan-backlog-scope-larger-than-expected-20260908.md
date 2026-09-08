# AC6 retail NTSC-U/J — r432 — correctif appliqué et vérifié : r430 double-présentait dans le chemin de test « secours » (`readback_==0`), trouvé par une reconstruction propre depuis zéro ; l'arriéré Vulkan est BIEN PLUS étendu que ce que r424 avait caractérisé — non committé, décision explicitement repoussée

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r431 (chaîne r399-r431 close, aucun blocage). Ligne ouverte
la moins chère nommée : l'audit dédié de l'arriéré Vulkan/registre de
shaders épinglés (r424), avec son fichier suspect
`native_vulkan_backend.cpp.new_header_part`.

## Établi

### Le fichier suspect est un travail réel, non fini, sans rapport avec ce qui bloque

`native_vulkan_backend.cpp.new_header_part` (79 lignes) : ajoute des
constantes de registres Xenos, des décalages de bloc de constantes
système, des décalages `SysFlag`, et une fonction `topology_of()` —
préparation pour un rendu réel piloté par les shaders épinglés
(remplacer l'effacement placeholder par de vrais tirages). **Diff
contre le fichier réel actuel** : ce contenu N'A PAS encore été
fusionné (le fichier réel a un en-tête bien plus court, sans ces
constantes). Rien dans le reste de l'arbre ne référence
`topology_of()` ou ces nouvelles constantes — travail authentique,
inachevé, jamais compilé (extension `.new_header_part`, pas `.cpp`).
**Décision : ne pas y toucher, ne pas le committer, ne pas tenter de
le terminer.**

### Les autres fichiers Vulkan déjà utilisés (`native_vulkan_device`, `native_pinned_shaders`) sont fonctionnels — déjà prouvé par r429-r431

Aucun marqueur `TODO`/`FIXME`/« not implemented » dans
`native_vulkan_device.h/.cpp` (1597 lignes) ni
`native_pinned_shaders.h/.cpp` (298 lignes) ; `native/tools/
materialize_pinned_shader_data.py` (générateur du fichier de données
au moment de la configuration, déjà cité par `CMakeLists.txt`) est
propre et complet. Ces fichiers ont déjà compilé et tourné avec succès
tout au long de r429-r431 — mais SANS reconstruction propre depuis
zéro.

### Une reconstruction VRAIMENT propre (reconfiguration + rebuild complet des 12 cibles) a trouvé une régression réelle dans le correctif de r430

`cmake -S ... -B ...` depuis une configuration effacée, puis
reconstruction complète (123 cibles) : succès. **`ctest` complet :
`ac6_native_xenos_tests` ÉCHOUE** —
`guest_vd_service_present_executes_offscreen` :
`assert(backend.present_count() == 2u)` échoue.

**Cause** : ce test exerce délibérément le chemin de secours de
`publish_write_address` (« the small, explicit unit-test/API fixture »,
commentaire déjà présent) — il construit lui-même un vrai paquet PM4
`XE_SWAP` et le fait décoder/exécuter via `drain_locked()`
(`present_count` attendu : `2`, un pour `submit()`, un pour
l'exécution). **Le correctif de r430 appelait
`present_to_offscreen` de façon INCONDITIONNELLE en tête de fonction**
— dans ce scénario de test précis, `present_target_`/`backend_` sont
bien liés, donc mon appel direct se déclenchait EN PLUS du présent
déjà compté par le chemin de décodage PM4, portant le compte à `3`.
**Un vrai double-présent dans un scénario que mes vérifications
ciblées de r430 (lancements du binaire réel uniquement) n'exerçaient
jamais** — seule une reconstruction complète + suite de tests
complète l'a révélé.

## Correctif appliqué

`native/src/native_guest_vd.cpp` — ajout de la garde `readback_ !=
0u` à la condition de l'appel direct de r430, la MÊME condition déjà
utilisée juste en dessous pour distinguer le chemin retail réel du
chemin de secours pour tests :

```c++
if (base_ != nullptr && base == base_ && backend_ != nullptr &&
    present_target_ != nullptr && readback_ != 0u) {
```

## Établi — vérifié en direct

- `ctest` complet après correctif : **11/11 réussis** (`15.36s`,
  `ac6_native_xenos_tests` inclus).
- Lancement réel (`AC6_NATIVE_VD_TRACE=1`, fenêtre `25000ms`) : **inchangé** —
  `present_count` toujours `1→5`, `presented_frames=5 state=2`.
- Lancement autonome de contrôle : `exit=0`, `elapsed=26s`,
  `presented_frames=5 state=2`.
- Gates du dépôt et `ctest` racine : tous verts, même échec
  préexistant sans rapport.

## L'arriéré est bien plus étendu que ce que r424 avait décrit — décision explicitement repoussée

`git status` sur `recompilation/ace-combat-6-retail/native/` révèle
un arriéré non committé qui dépasse largement les fichiers nommés par
r424 (Vulkan/registre de shaders épinglés) : `native_guest_media.h/.cpp`,
`native_guest_memory.h/.cpp`, `native_shader_translator.h/.cpp`,
`native_xenos.h/.cpp`, et des fichiers de test, sont TOUS modifiés de
façon non committée, en plus de `native_vulkan_backend.h/.cpp` et des
fichiers déjà identifiés. **Ce n'est plus « l'arriéré Vulkan », c'est
une fraction substantielle de tout l'arbre `native/`.**

**Décision explicite** : ne PAS étendre ce cycle à un committage en
masse de cet arriéré élargi. La situation d'arbre non committé reste,
comme documenté dans chaque rapport de cette chaîne depuis r423, « la
décision de l'utilisateur, non touchée » — et cette découverte montre
que la portée réelle est nettement plus grande que ce qu'un cycle de
continuation autonome devrait committer sans consultation explicite.
Seul le correctif ciblé de ce cycle (`native_guest_vd.cpp`, déjà
committé par r430 avec divulgation) est étendu ici.

## Non établi

- **L'étendue précise de cet arriéré élargi** (quels fichiers, quels
  cycles, quel volume) — non cataloguée en détail ce cycle, seulement
  constatée par `git status`.
- **Si d'autres régressions similaires** (comme celle trouvée ici)
  se cachent dans le reste de l'arriéré non committé — seule la
  reconstruction complète a révélé celle-ci ; rien ne garantit qu'il
  n'y en a pas d'autres.

## Décisions prises

- Committer UNIQUEMENT le correctif de régression ciblé
  (`native_guest_vd.cpp`), pas l'arriéré élargi découvert en cours de
  route — portée disproportionnée à ce qu'un cycle de continuation
  autonome devrait décider seul (précédent : « la situation d'arbre
  non committé reste la décision de l'utilisateur », répété dans
  chaque rapport depuis r423).
- Faire une reconstruction VRAIMENT propre (reconfiguration CMake
  depuis zéro, pas seulement `ninja` incrémental) avant de considérer
  committer quoi que ce soit de plus large — c'est elle qui a trouvé
  la régression que les vérifications ciblées de r430 avaient
  manquée.

## Gate

**Source de production éditée ce cycle** :
`recompilation/ace-combat-6-retail/native/src/native_guest_vd.cpp`
(déjà divulgué comme portant un arriéré préexistant depuis r430 ; ce
cycle n'ajoute que la garde `readback_ != 0u`, pas de nouvel arriéré).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus)
`ctest` (racine et natif, reconstruction complète) relancés en entier.

## Named for r433

Si l'utilisateur souhaite poursuivre l'audit de l'arriéré `native/`
élargi : le cataloguer précisément (quels fichiers, quels cycles y
sont cités) avant toute décision de committer, fichier par fichier
plutôt qu'en bloc — chaque fichier mériterait la même rigueur
(reconstruction complète + suite de tests) que celle qui a révélé la
régression de ce cycle.

## Files

Aucun artefact gitignoré nouveau (journaux de vérification sous
`/tmp`, non conservés).

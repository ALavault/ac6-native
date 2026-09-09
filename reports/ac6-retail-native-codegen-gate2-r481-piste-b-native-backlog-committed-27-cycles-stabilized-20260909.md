# AC6 retail NTSC-U/J — r481 — Piste B (plan approuvé) : l'arriéré source natif stabilisé sur 27 cycles (r454-r480) est enfin committé — vérifié identique avant/après, aucune régression

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Piste B du plan approuvé (`/home/lavaulta/.claude/plans/groovy-beaming-book.md`,
« Piste B — committer l'arriéré natif stabilisé (r433-r475/r480) »),
décision déjà prise explicitement par l'utilisateur. Piste A vient de
se mettre en pause à r480 (5 cycles consécutifs de rendements
décroissants) ; cette piste indépendante était prête à démarrer.

## Établi — revue et committage

`git status --porcelain native/` : 17 fichiers modifiés + 5 fichiers
non suivis (`native_pinned_shaders.{h,cpp}`, `native_vulkan_device.{h,cpp}`,
`tools/materialize_pinned_shader_data.py`), 8070 lignes de diff sur
l'arbre de travail. Un fichier de scratch orphelin a été trouvé et
**délibérément exclu** :
`native/src/native_vulkan_backend.cpp.new_header_part` (79 lignes, un
brouillon d'en-tête d'une cycle très ancienne — bien antérieur à
l'état actuel du fichier de 3000+ lignes — resté sur le disque sans
être committé ni supprimé).

Recherche de secrets sur le diff complet et les fichiers non suivis
(`grep -iE "password|secret|api.?key|token|BEGIN (RSA|PRIVATE|OPENSSH)"`) :
**aucun résultat**. Les trois traces de diagnostic ajoutées pendant
la campagne (`r459:`, `r461 probe:`, `r478 probe:` dans
`native_vulkan_backend.cpp`) ont été vérifiées : toutes les trois sont
gardées par `std::getenv("AC6_NATIVE_VD_TRACE") != nullptr` — une
infrastructure de diagnostic permanente et réutilisée tout au long de
la campagne (même patron que `AC6_NATIVE_CAPTURE`), pas des impressions
de débogage temporaires à retirer.

**Granularité de commit** : un seul commit synthétique plutôt que
plusieurs commits par sous-système. Les fichiers sont fortement
interdépendants (les nouveaux `native_pinned_shaders.h`/
`native_vulkan_device.h` sont inclus par `native_vulkan_backend.cpp` ;
`native_runtime.h` dépend du type `PinnedShaderRuntime` ;
`CMakeLists.txt` doit référencer les nouveaux fichiers source pour que
quoi que ce soit compile) — les séparer sans vérifier que chaque état
intermédiaire compile aurait risqué un historique non bisectable, sans
budget disponible pour vérifier N builds intermédiaires ce cycle.

## Établi — vérifié identique avant ET après le commit

Triptyque de vérification complet exécuté deux fois (avant committage,
sur l'arbre de travail ; après committage, sur l'état committé) :
- Les trois audits de contrats : `mission01_final_gate=audit-valid
  JF=pass`, `contract_artifacts=pass contracts=6 cited=189
  match_head=189`, `contract_addresses=pass contracts=6 cited=321
  supported=321 unsupported=0` — identiques avant/après.
- `ctest` natif : **11/11** avant, **11/11** après.
- `ctest` racine : seul l'échec préexistant `ac6-cpp-complexity` (sans
  rapport), identique avant/après.
- Sondage `--probe-entry` réel (`AC6_NATIVE_ALLOW_ENTRY_PROBE=1
  AC6_NATIVE_VD_TRACE=1 AC6_NATIVE_CAPTURE=1
  AC6_NATIVE_PROBE_WINDOW_MS=25000`, ISO réelle) : `presented_frames=5
  state=2`, capture `pixels=921600 non_black=0 distinct_colors=1` —
  **identique avant/après**, et identique à la référence connue de
  toute la campagne r454-r480 (Piste A reste ouverte sur l'obtention
  de contenu visible réel, non affectée par ce commit).

Commit : `7dec3f57` (« feat: wire PinnedShaderRuntime into the native
Vulkan present path (r454-r480 backlog) »), 23 fichiers, +10012/-46
lignes (dont 5 nouveaux fichiers créés).

## Non établi

- **Si un découpage plus fin serait possible** pour un futur arriéré
  similaire — non tenté ce cycle, la coupure d'interdépendance rendait
  le risque disproportionné au bénéfice pour ce lot précis.

## Décisions prises

- Un seul commit synthétique plutôt qu'un découpage par sous-système,
  pour éviter un historique non-bisectable sans budget de vérification
  suffisant.
- Exclure le fichier de scratch orphelin (`*.new_header_part`) sans le
  supprimer — laissé sur le disque, hors du commit, pour qu'une
  décision de suppression reste explicite plutôt qu'implicite dans ce
  commit.

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
Tous verts, avant et après committage. `ctest` natif 11/11 et `ctest`
racine (seul `ac6-cpp-complexity`, préexistant) inchangés avant/après.

## Named for r482

Piste B est close. Reste ouvert : Piste C du plan approuvé (garde-fou
HUD, `reconstruction/ace-combat-6`, indépendante) — prête à démarrer ;
Piste A (campagne oracle ciblée écran-titre) reste en pause à r480 sur
recommandation explicite, à reprendre seulement sur décision
utilisateur ; le fichier de scratch orphelin
(`native/src/native_vulkan_backend.cpp.new_header_part`) reste sur le
disque, non committé — à supprimer ou committer sur une décision
explicite future.

## Files

Aucun artefact gitignoré nouveau.

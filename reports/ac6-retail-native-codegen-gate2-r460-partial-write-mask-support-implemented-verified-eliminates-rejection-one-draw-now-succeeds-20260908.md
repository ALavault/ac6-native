# AC6 retail NTSC-U/J — r460 — support des masques d'écriture RB_COLOR_MASK partiels implémenté dans `PinnedShaderRuntime` : traduction mécanique en `colorWriteMask` Vulkan, vérifié sans régression, élimine son propre motif de rejet et fait réussir un tirage de plus

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r459, piste 2 : le motif de rejet apparu ce cycle-là
« partial render-target write masks are not qualified this cycle »
(`derive_edram_render_target()`), sans rapport avec MSAA, non
investigué.

## Établi — un vrai diagnostic en direct, pas une supposition

`derive_edram_render_target()` exigeait `RB_COLOR_MASK & 0xF == 0xF`
(les quatre composantes RGBA doivent être actives). Une trace de
diagnostic locale (gardée par `AC6_NATIVE_VD_TRACE`) a montré la
valeur réelle rencontrée :
```
r460 probe: RB_COLOR_MASK=0x00000000 masked_bits=0
```
Le masque est intégralement à zéro — ce tirage n'écrit AUCUNE
composante couleur dans RT0. `RB_DEPTHCONTROL` est déjà exigé à zéro
par la garde suivante (aucune surface de profondeur modélisée), donc
ce n'est pas, dans le modèle actuel de ce moteur, un pré-passage de
profondeur seule ; quel que soit le rôle réel du jeu pour ce tirage,
c'est un état matériel authentique, pas une valeur corrompue.

## Correctif appliqué et vérifié (local, non committé — même statut que r454-r459)

Contrairement à la géométrie de tuile EDRAM sous MSAA (r459, laissée
délibérément non résolue faute de preuve citable), la traduction d'un
masque RB_COLOR_MASK vers `VkPipelineColorBlendAttachmentState::
colorWriteMask` est **mécanique et sans ambiguïté** : R=bit0, G=bit1,
B=bit2, A=bit3, préservant directement l'ordre des composantes déjà
utilisé ailleurs dans ce fichier pour d'autres champs de registre. Ce
cycle :
- `EdramRenderTarget` porte maintenant `color_mask` (0x0 à 0xF, capturé
  sans rejet).
- `PipelineKey` inclut `color_mask` (comme `sample_count` en r459) —
  un masque différent nécessite un objet pipeline Vulkan différent
  (état statique, pas de state dynamique `VK_DYNAMIC_STATE_COLOR_
  WRITE_MASK_EXT` utilisé ici).
- `ensure_pipeline()` traduit les 4 bits en `VK_COLOR_COMPONENT_*_BIT`
  au lieu de forcer les quatre composantes.
- La vérification de rejet est supprimée entièrement — toutes les 16
  combinaisons de masque sont maintenant acceptées.

## Établi — vérifié sans régression, élimine son propre motif de rejet, ET fait réussir un tirage de plus

`ctest` natif complet : **11/11** (dont `ac6_native_xenos_tests`,
aucune régression). Relancé contre l'ISO réelle avec trace :
```
7 × « no pinned variant matches this draw state » (inchangé, registre épinglé)
1 × « render target region exceeds the bounded size » (nouveau, sans rapport)
presented_frames=5 state=2 (inchangé)
capture pixels=921600 non_black=0 distinct_colors=1 (inchangé)
```
**Le motif « partial render-target write masks » a disparu (0
occurrence)**. Le compte total de replis est passé de **9 à 8** : un
tirage qui échouait auparavant réussit maintenant réellement à être
épinglé (exécuté via `execute_frame()`, pas la validation structurelle
de repli) — cohérent avec le fait que le tirage à masque `0x0` observé
n'écrit par construction aucun pixel visible (`colorWriteMask=0`),
donc `non_black`/`distinct_colors` restent inchangés alors même que le
correctif fonctionne correctement : le moteur épinglé rend maintenant
ce tirage fidèlement (rien à l'écran, comme le matériel réel
produirait), au lieu de le rejeter et de faire retomber tout le lot
sur la validation structurelle pure (le mode de défaillance déjà
identifié par r454).

## Non établi

- **Le rôle réel du jeu pour un tirage à masque entièrement nul** dans
  ce contexte précis (aucune surface de profondeur modélisée par ce
  moteur) — non investigué, hors périmètre.
- **Le nouveau motif « render target region exceeds the bounded
  size »** (une limite déjà existante, `kMaxEdramRegionWidth`/
  `kMaxEdramRegionHeight` = 4096, non touchée ce cycle) — apparu une
  fois, non investigué.

## Décisions prises

- Implémenter ce correctif ce cycle (contrairement au questionnement
  de géométrie MSAA de r459) car la traduction bit-à-bit est
  mécanique, déjà utilisée ailleurs dans ce fichier pour des champs de
  registre similaires, et ne repose sur aucune supposition de
  disposition mémoire non vérifiée.
- Toujours ne pas committer — `native_vulkan_backend.{h,cpp}` restent
  entremêlés avec l'arriéré déjà catalogué.

## Gate

**Aucune source committée ce cycle** — le correctif reste appliqué
localement, vérifié (build + `ctest` 11/11 + lancement réel avec
trace), non committé.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées — aucune source de production committée ce cycle)
`ctest` racine inchangé (aucune source de production committée ce
cycle).

## Named for r461

Deux pistes, ni l'une ni l'autre bloquante : (1) le motif de rejet du
registre épinglé (couverture des 271 variantes, r456, maintenant le
SEUL motif dominant restant — 7 des 8 replis) nécessite probablement
une session oracle — décision utilisateur, pas à prendre seul ; (2) le
nouveau motif « render target region exceeds the bounded size »,
apparu une fois, non investigué — pourrait être un simple
élargissement de limite ou un vrai trou de couverture, à déterminer.
Reste ouvert sinon : une fois le câblage jugé mûr, reconsidérer le
committage groupé de l'arriéré (r433/r434/r438/r454-r460) ; mise à
jour de `tools/prepare.py`/`tools/build.py` pour le chemin ISO par
défaut (confort, pas une nécessité).

## Files

Aucun artefact gitignoré nouveau (journal de vérification sous
`/fastdata/tmp/...scratchpad/`, non conservé).

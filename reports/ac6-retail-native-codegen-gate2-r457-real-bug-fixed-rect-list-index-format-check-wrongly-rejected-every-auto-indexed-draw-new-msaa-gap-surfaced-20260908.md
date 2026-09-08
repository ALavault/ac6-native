# AC6 retail NTSC-U/J — r457 — un vrai bug corrigé : la vérification `index_format` de l'expansion de listes de rectangles rejetait TOUT tirage auto-indexé par construction (jamais un problème de couverture de registre) — corrigé et vérifié, un NOUVEAU motif de rejet honnêtement révélé (« MSAA render targets are not qualified »)

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r456, piste 1 : étendre la couverture du registre épinglé
pour les deux motifs de rejet caractérisés. Le second motif
(« rectangle-list expansion is not qualified with 16-bit guest
indices ») ne nécessitait pas de session oracle (contrairement au
premier, « no pinned variant matches this draw state », qui
nécessiterait de nouvelles traductions issues de l'oracle Xenia,
hors périmètre autonome de ce cycle) — investigué en premier.

## Établi — ce n'était pas une limitation de couverture, mais un vrai bug de garde

Lecture de `draw_pinned()` (`native/src/native_vulkan_backend.cpp`) :
```c++
const bool indexed_draw = draw.index_address != 0u;
...
if (rect_strip_expand) {
  if (draw.index_format != 1u) {   // <- rejette SANS CONDITION
    error_ = "... not qualified with 16-bit guest indices ...";
    return false;
  }
  if (indexed_draw) { /* scanne le vrai tampon d'index invité */ }
  /* génère les index de bande hôte — INCONDITIONNEL, indépendant de indexed_draw */
  ...
}
```
**La vérification `draw.index_format != 1u` s'appliquait sans
condition à TOUT tirage de type liste de rectangles** — y compris les
tirages **auto-indexés** (`index_address == 0`, donc `indexed_draw =
false`), pour lesquels `index_format` vaut TOUJOURS `0` par
construction (`native_xenos.cpp`, décodage `DRAW_INDX_2` :
`DrawPacket{..., 0u /*index_address*/, ..., 0u /*index_format*/,
...}`). **Or r435 avait déjà établi, en direct, que TOUS les tirages
réels observés cette session sont auto-indexés**
(`index_address=0x00000000`). Conséquence : **aucun tirage de liste
de rectangles réel ne pouvait JAMAIS être épinglé**, quelle que soit
la couverture du registre de nuanceurs — un rejet garanti par la
structure du code, pas par un manque de traductions oracle.

La génération des index de bande hôte (la vraie logique d'expansion,
juste après cette vérification) s'exécute déjà sans condition à
l'intérieur du bloc `if (rect_strip_expand)`, **indépendamment de**
`indexed_draw` — seule la branche `if (indexed_draw)` juste après lit
le vrai tampon d'index invité. `index_format` n'a donc aucune
pertinence quand `indexed_draw` est faux.

## Correctif appliqué et vérifié (local, non committé — même statut que r454-r456)

```c++
if (indexed_draw && draw.index_format != 1u) {   // gardé désormais par indexed_draw
  error_ = "... not qualified with 16-bit guest indices ...";
  return false;
}
```
Préserve exactement le comportement existant pour les tirages
DMA-indexés (toujours 16 bits requis), lève le rejet pour les tirages
auto-indexés (où `index_format` est sans objet).

**Vérifié** : `ctest` natif complet **11/11** (y compris
`ac6_native_xenos_tests`, qui couvre extensivement les listes de
rectangles depuis r262-r270 — aucune régression). Relancé contre
l'ISO réelle avec trace : **le motif de rejet « 16-bit guest
indices » a disparu des replis observés** — le correctif fonctionne
exactement comme prévu.

## Établi — un nouveau motif de rejet honnêtement révélé, pas caché

Sur ce même run, 9 replis toujours enregistrés, mais avec des motifs
différents cette fois : `« no pinned variant matches this draw
state »` (déjà connu, nécessite l'oracle) et **un nouveau** : `«
MSAA render targets are not qualified this cycle »`. La capture reste
`non_black=0 distinct_colors=1` (toujours un noir uniforme, sans
régression — le correctif de r456 continue de garantir une image
définie). **Le correctif de ce cycle a résolu son problème ciblé sans
produire de contenu visible immédiat, parce que d'autres limitations
de couverture existent encore** — rapporté honnêtement plutôt que de
laisser croire que le correctif suffisait à tout résoudre.

## Non établi

- **Ce que « MSAA render targets are not qualified » recouvre
  précisément** — non investigué ce cycle.
- **Si un tirage de liste de rectangles a RÉELLEMENT été épinglé avec
  succès** sur ce run (les tirages `vd draw` observés dans la trace de
  ce cycle semblent être des tirages à 1 sommet, pas des listes de
  rectangles) — le correctif est vérifié correct par construction et
  par les tests, mais pas encore observé en train de RÉUSSIR un vrai
  tirage de ce type en direct.

## Décisions prises

- Corriger le second motif de rejet (pas de session oracle
  nécessaire) plutôt que le premier — respecte la discipline « ne pas
  dépenser le budget oracle sans décision explicite de l'utilisateur »
  déjà établie pour toute cette campagne.
- Rapporter le nouveau motif MSAA honnêtement plutôt que de
  s'arrêter sur le succès partiel du correctif — cohérent avec la
  discipline de ce dépôt de ne jamais cacher ce qui reste non résolu.
- Toujours ne pas committer — `native_vulkan_backend.cpp` est le
  fichier le PLUS entremêlé avec l'arriéré déjà catalogué (r433,
  +2887 lignes non committées).

## Gate

**Aucune source committée ce cycle** — le correctif reste appliqué
localement, vérifié (build + `ctest` 11/11 + lancement réel avec
trace + capture), non committé.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus, inchangées — aucune source de
production committée ce cycle)
`ctest` racine inchangé (aucune source de production committée ce
cycle).

## Named for r458

Investiguer le nouveau motif « MSAA render targets are not qualified
this cycle » — probablement un autre correctif ciblé sans besoin
d'oracle, sur le même modèle que ce cycle. Reste ouvert sinon : (1)
le premier motif de rejet caractérisé par r456 (couverture du
registre épinglé) nécessite probablement une session oracle — décision
utilisateur, pas à prendre seul ; (2) une fois le câblage jugé mûr,
reconsidérer le committage groupé de l'arriéré
(r433/r434/r438/r454-r457) ; (3) mise à jour de
`tools/prepare.py`/`tools/build.py` pour le chemin ISO par défaut
(r452/r453, confort, pas une nécessité).

## Files

Aucun artefact gitignoré nouveau (journaux de vérification sous
`/fastdata/tmp/...scratchpad/`, non conservés).

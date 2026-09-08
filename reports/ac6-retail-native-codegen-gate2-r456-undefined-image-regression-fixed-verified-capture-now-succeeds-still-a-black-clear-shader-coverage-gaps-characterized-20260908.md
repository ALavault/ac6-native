# AC6 retail NTSC-U/J — r456 — la régression d'image indéfinie de r455 est CORRIGÉE et vérifiée : la capture réussit maintenant (1280×720, image bien définie) — toujours un effacement noir uniforme sur ce run, cause caractérisée : les tirages réels rencontrés ne correspondent à aucune des 271 variantes du registre épinglé (deux motifs de rejet distincts, déjà documentés dans le code existant)

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r455 : garantir une image définie sur le chemin de présent
direct même quand aucun tirage épinglé n'a réussi, sans réintroduire
le double-effacement/double-présent déjà corrigé (r432/r454), puis
relancer la capture.

## Correctif appliqué et vérifié (local, non committé — même statut que r454/r455)

`native/src/native_guest_vd.cpp`, `publish_write_address()` : détecte
si `execute_frame()` a réellement effectué une résolution EDRAM via
le delta de `pinned_->edram_resolves()` (le seul signal que
`PinnedShaderRuntime` expose déjà pour « une résolution a-t-elle eu
lieu ») — `execute_frame()` incrémente `present_count()` sans
condition même sans cible EDRAM active, donc un présent « réussi »
n'est pas la preuve que l'image a été touchée. Si aucune résolution
n'a eu lieu, appelle `present_target_->clear(...)` **directement**
(pas via `backend_->present_to_offscreen()`, qui doublerait le
compte dans la somme additive `backend_+pinned_` de `diagnostics()`,
r455) — un effacement de secours qui ne touche aucun compteur de
présent.

## Établi — vérifié en direct, la capture réussit maintenant

`ctest` natif : 11/11. Lancement contre l'ISO réelle avec
`AC6_NATIVE_CAPTURE=1` :
```
ac6recomp: presented_frames=5 state=2
ac6recomp: capture pixels=921600 non_black=0 distinct_colors=1
```
`921600 = 1280×720` — dimensions confirmées correctes.
**La capture réussit** (plus d'erreur « not in a blittable layout ») —
la régression de r455 est corrigée. `non_black=0 distinct_colors=1` :
l'image entière est un noir uniforme — **exactement le comportement
de l'ancien chemin `backend_->present_to_offscreen()`**, préservé
sans régression, plutôt qu'un plantage ou une image indéfinie.

## Établi — pourquoi le contenu réel n'apparaît toujours pas ce cycle

Relancé avec `AC6_NATIVE_VD_TRACE=1` en plus : **9 replis** sur cette
fenêtre, deux motifs de rejet distincts, tous deux déjà des
limitations connues et documentées dans le code de
`PinnedShaderRuntime` (pas de nouveaux bugs) :
1. `« active shader is not pinned: no pinned variant matches this
   draw state »` — la combinaison de nuanceurs de ce tirage n'est
   simplement pas l'une des 271 traductions issues de l'oracle
   (r255) — un registre non exhaustif, par conception.
2. `« rectangle-list expansion is not qualified with 16-bit guest
   indices this cycle »` — une limitation déjà nommée dans le code
   (r265) : l'expansion de listes de rectangles n'est qualifiée que
   pour des index invité 16 bits, ce tirage utilise autre chose.

## Ce que ceci établit

**La chaîne de correctifs r454→r456 est maintenant cohérente et sans
régression connue** : le vrai moteur de rendu est câblé, un compteur
de présent correct, une image toujours bien définie (jamais pire que
l'ancien comportement), et — sur ce run précis — aucun tirage réel ne
correspond au registre épinglé, ce qui est une limitation de
couverture caractérisée, pas un défaut du câblage lui-même.

## Non établi

- **Si un lancement différent** (autre timing, autre séquence de
  démarrage) produirait un tirage qui correspond réellement au
  registre épinglé — non testé sur plusieurs lancements ce cycle.
- **L'étendue de la couverture du registre de 271 variantes** par
  rapport à l'ensemble des combinaisons de nuanceurs que ce titre
  utilise réellement — hors périmètre de ce cycle.

## Décisions prises

- Utiliser `edram_resolves()` comme signal plutôt que le simple
  succès de `execute_frame()` — le seul signal fiable déjà exposé
  par `PinnedShaderRuntime` pour distinguer « a réellement résolu
  quelque chose » de « n'a rien fait mais n'a pas échoué non plus ».
- Ne pas tenter d'élargir la couverture du registre épinglé ce
  cycle — un travail distinct (probablement lié à l'oracle Xenia déjà
  utilisé pour peupler ce registre, r255), hors périmètre d'une
  correction de régression.
- Toujours ne pas committer — même entremêlement avec l'arriéré déjà
  catalogué que r454/r455.

## Gate

**Aucune source committée ce cycle** — le correctif reste appliqué
localement, vérifié (build + `ctest` 11/11 + deux lancements réels
avec capture), non committé.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus, inchangées — aucune source de
production committée ce cycle)
`ctest` racine inchangé (aucune source de production committée ce
cycle).

## Named for r457

Deux pistes, ni l'une ni l'autre bloquante : (1) étendre la
couverture du registre épinglé pour les deux motifs de rejet
caractérisés ce cycle (nouvelles traductions oracle, ou qualifier
l'expansion 16 bits pour d'autres formats d'index) — travail réel
distinct, probablement son propre cycle dédié ; (2) une fois le
câblage jugé suffisamment mûr et stable (r454-r456 maintenant sans
régression connue), reconsidérer le committage groupé de l'arriéré
(r433/r434/r438/r454/r455/r456). Reste ouvert sinon : mise à jour de
`tools/prepare.py`/`tools/build.py` pour le chemin ISO par défaut
(r452/r453, confort, pas une nécessité).

## Files

Aucun artefact gitignoré nouveau (journaux de vérification sous
`/fastdata/tmp/...scratchpad/`, non conservés).

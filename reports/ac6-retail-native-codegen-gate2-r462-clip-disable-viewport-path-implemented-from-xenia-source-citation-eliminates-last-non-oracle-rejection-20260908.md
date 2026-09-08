# AC6 retail NTSC-U/J — r462 — le chemin de viewport `clip_disable` implémenté à partir d'une citation directe du code source Xenia public (pas un run d'oracle) — élimine le DERNIER motif de rejet non lié au registre épinglé ; il ne reste plus qu'un seul motif, qui nécessite une décision utilisateur

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé (lecture du code source public de Xenia sur
GitHub — pas une exécution de Xenia comme oracle N3 ; distinction déjà
établie par le cycle 399, qui citait ce même fichier).

## Contexte

Nommé par r461, piste 2 : la garde `clip_disable viewport path not
qualified this cycle` (`derive_edram_render_target()`/`draw_pinned()`,
ligne 2771), nouvellement atteignable depuis le correctif de bornage
du scissor de r461, jamais investiguée.

## Établi — la formule exacte vient du code source Xenia public, pas d'une supposition

`PA_CL_CLIP_CNTL::clip_disable` (bit 16) était rejeté sans condition.
Le code déjà présent dans ce fichier (le chemin `clip` activé)
commentait déjà « l'oracle's `GetHostViewportInfo` derivation » sans
jamais implémenter la branche `clip_disable`. Plutôt que deviner cette
branche, son code source EXACT a été récupéré directement depuis le
dépôt public `xenia-project/xenia` (branche `master`) :

- `src/xenia/gpu/draw_util.cc:362-386` — la fonction
  `GetHostViewportInfo`, branche `pa_cl_clip_cntl.clip_disable` :
  utilise une étendue FIXE (`extent_axis_unscaled = min(8192,
  xy_max_unscaled[i])`) pour remapper les coordonnées pixel que le
  vertex shader produit directement (sans division par clip) vers le
  NDC, au lieu de l'étendue dérivée de la cible de rendu que la
  branche activée utilise.
- `src/xenia/gpu/vulkan/vulkan_command_processor.cc:2440-2444` — le
  point d'appel Vulkan de Xenia : `x_max`/`y_max` sont
  `device_properties.maxViewportDimensions[0]`/`[1]`, **la limite du
  PÉRIPHÉRIQUE HÔTE**, PAS la taille de la cible de rendu — un piège
  facile à deviner de travers (une lecture naïve de « x_max » suggère
  une dimension de cible, alors que c'est une capacité matérielle
  hôte).
- `src/xenia/gpu/xenos.h:1139-1141` —
  `kTexture2DCubeMaxWidthHeight = 1 << 13 = 8192`, la borne fixe
  (déjà confirmée indépendamment par le cycle 399 de ce même dépôt :
  « échelle NDC 0.000244140625 = 2/8192 »).

## Correctif appliqué et vérifié (local, non committé — même statut que r454-r461)

La construction commune (`scale_xy`/`offset_base_xy`/`offset_add_xy`,
déjà partagée par le chemin activé) est factorisée avant la
bifurcation `clip_disable`. La nouvelle branche interroge
`VkPhysicalDeviceLimits::maxViewportDimensions` du périphérique réel
(pas une constante codée en dur), la borne à 8192 comme le fait
Xenia, puis applique EXACTEMENT la formule citée :
`ndc_scale[axis] = scale_xy[axis] * (2/extent)`,
`ndc_offset[axis] = (offset_base_xy[axis] - extent*0.5 +
offset_add_xy[axis]) * (2/extent)`. Le viewport/scissor Vulkan
(`vkCmdSetViewport`/`Scissor`) restent inchangés — ils utilisent déjà
la région dérivée du scissor (r461), correspondant exactement à
l'exigence de Xenia (« utiliser un viewport au moins aussi grand que
la région de scissor »).

## Établi — vérifié sans régression, élimine le DERNIER motif de rejet indépendant du registre épinglé

`ctest` natif complet : **11/11** (dont `ac6_native_xenos_tests`,
aucune régression). Relancé contre l'ISO réelle avec trace :
```
9 × « no pinned variant matches this draw state » (registre épinglé, r456)
presented_frames=5 state=2 (inchangé)
capture pixels=921600 non_black=0 distinct_colors=1 (inchangé)
```
**Le motif « clip_disable viewport path not qualified this cycle » a
disparu (0 occurrence)**. Fait notable : **les 9 replis de ce run
portent désormais TOUS le même motif unique** — le trou de couverture
du registre épinglé (271 variantes, r456). Chaque autre motif de rejet
rencontré depuis r457 (index de listes de rectangles, MSAA, masque
d'écriture partiel, scissor démesuré, `clip_disable`) est maintenant
éliminé, chacun par un correctif mécanique vérifié sans preuve
manquante — sauf la géométrie de tuile EDRAM sous MSAA (r459),
délibérément non résolue faute de citation.

## Non établi

- **Si des tirages MSAA réels rencontrent la question de géométrie de
  tuile laissée ouverte par r459** — aucun tirage de ce run n'atteint
  cette limite (tous bloqués par le registre épinglé en amont).
- **Si un contenu visible apparaîtra une fois la couverture du
  registre étendue** — dépend d'un travail hors du périmètre de ce
  cycle (voir décisions ci-dessous).

## Décisions prises

- Consulter le code source public de Xenia (lecture GitHub, pas un run
  d'oracle N3) plutôt que deviner la formule — cohérent avec la
  méthodologie déjà établie par le cycle 399 de ce même dépôt.
- Toujours ne pas committer — `native_vulkan_backend.cpp` reste
  entremêlé avec l'arriéré déjà catalogué.
- **Ne pas continuer à chercher d'autres correctifs mécaniques sans
  oracle ce cycle** : le seul motif de rejet restant (registre épinglé)
  est déjà caractérisé depuis r456 comme nécessitant probablement une
  session oracle Xenia (les 271 traductions de nuanceurs viennent de
  là, r255) — une décision de dépense de budget N3 qui revient à
  l'utilisateur, pas à prendre seule. C'est un point d'arrêt naturel
  pour cette chaîne d'investigation.

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

## Named for r463

**Un seul motif de rejet restant, et il pointe vers une décision
utilisateur** : étendre la couverture du registre de nuanceurs épinglé
(271 variantes, r456) nécessite probablement une nouvelle session
oracle Xenia — c'est un blocage qualifié au sens de `CLAUDE.md` (dépense
d'un budget N3 réservé), pas à prendre unilatéralement. Reste ouvert,
non bloquant : (1) une fois le câblage jugé mûr (r454-r462, maintenant
sans régression connue et sans motif de rejet mécaniquement corrigible
restant), reconsidérer le committage groupé de l'arriéré
(r433/r434/r438/r454-r462) ; (2) mise à jour de
`tools/prepare.py`/`tools/build.py` pour le chemin ISO par défaut
(confort, pas une nécessité).

## Files

Aucun artefact gitignoré nouveau (journal de vérification sous
`/fastdata/tmp/...scratchpad/`, non conservé ; source Xenia
téléchargée temporairement pour lecture, non conservée non plus).

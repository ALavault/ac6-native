# AC6 retail NTSC-U/J — r459 — support d'une cible EDRAM multi-échantillonnée implémenté dans `PinnedShaderRuntime` : image Vulkan MS + résolution explicite à la présentation, vérifié sans régression, élimine son propre motif de rejet — un point de conception explicitement laissé non vérifié

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r458 : dimensionner et implémenter le support d'une cible
EDRAM 4× MSAA dans `PinnedShaderRuntime` (image Vulkan multisample +
résolution), reconnu comme du vrai travail de moteur de rendu plutôt
qu'un correctif d'une ligne.

## Décision de conception centrale (avant tout code)

Deux approches Vulkan standard existent pour résoudre une cible
multi-échantillonnée : (a) une résolution automatique de sous-passe
(`pResolveAttachments`), déclenchée à la fin de CHAQUE passe de rendu,
ou (b) une résolution explicite (`vkCmdResolveImage`) déclenchée une
seule fois, au moment choisi par l'appelant. Le modèle existant de ce
fichier ouvre une NOUVELLE passe de rendu par appel `draw_pinned()`
(pas une passe par frame), et plusieurs tirages s'accumulent sur la
même cible EDRAM avant la résolution finale au `PresentPacket`
(persistance déjà modélisée par `edram_rt_valid_`/`edram_active_rt_`
pour le cas 1×). Avec l'option (a), les échantillons non encore
résolus d'un tirage seraient perdus dès la fin de SA PROPRE passe
(stockage `DONT_CARE` de l'attachement multi-échantillonné),
empêchant toute accumulation correcte entre tirages successifs sur la
même cible. **L'option (b) a donc été retenue** : `edram_image_`
devient l'image multi-échantillonnée elle-même, persistante entre
tirages exactement comme l'image 1× existante (mêmes sémantiques
clear/load), et la résolution vers une image 1× lisible
(`edram_resolve_image_`, nouvelle) n'a lieu qu'une fois, explicitement,
dans `resolve_edram_to_target()` — au même endroit où la résolution
1× existante se produisait déjà.

## Établi — implémentation

- `EdramRenderTarget` porte maintenant `sample_count` (1, 2 ou 4,
  dérivé des bits 16:17 de `RB_SURFACE_INFO`, l'encodage Xenos
  `MSAA_NumSamples`), inclus dans `operator==` généré par défaut, donc
  un changement de nombre d'échantillons seul invalide correctement la
  surface EDRAM en cache et force un nouveau clear.
- `derive_edram_render_target()` : la vérification qui rejetait
  systématiquement tout `msaa_bits != 0` est remplacée par le calcul de
  `sample_count` (1×/2×/4× qualifiés, 8× toujours refusé
  explicitement — jamais observé cette campagne).
- `ensure_edram_passes(format, sample_count, ...)` : l'attachement
  unique existant devient simplement multi-échantillonné
  (`vk_sample_count(sample_count)`) ; **aucun second attachement n'est
  ajouté** (cohérent avec la décision de conception ci-dessus) — il
  reste légal de terminer un attachement couleur multi-échantillonné
  en `TRANSFER_SRC_OPTIMAL` (`vkCmdResolveImage` accepte une source
  multi-échantillonnée dans cette disposition).
- `ensure_edram_surface()` : `edram_image_` est créée avec
  `samples = vk_sample_count(rt.sample_count)` ; quand
  `sample_count > 1`, une image compagnon 1× (`edram_resolve_image_`,
  mêmes dimensions/format, usage `TRANSFER_DST|TRANSFER_SRC`, jamais
  utilisée comme attachement de passe) est créée pour servir de
  destination de résolution. Le cache d'invalidation compare
  maintenant aussi `sample_count`.
- `resolve_edram_to_target()` : quand `rt.sample_count > 1`, insère
  `vkCmdResolveImage(edram_image_ → edram_resolve_image_)` (avec les
  transitions de disposition `UNDEFINED→TRANSFER_DST_OPTIMAL` puis
  `→TRANSFER_SRC_OPTIMAL` autour) avant l'étape copy/blit existante
  vers la cible de présentation, qui lit alors `edram_resolve_image_`
  au lieu de `edram_image_` — le reste du chemin (copy pour
  `R8G8B8A8_UNORM`, blit pour les formats flottants) est **totalement
  inchangé**.
- `ensure_pipeline()` : `multisample.rasterizationSamples` suit
  maintenant `sample_count` ; `PipelineKey` inclut `sample_count`
  (avec repli sur le hash) pour un cache de pipeline correct par
  nombre d'échantillons.
- Nettoyage (`~PinnedShaderRuntime`) étendu pour détruire les nouvelles
  ressources de résolution.
- **Le cas `sample_count == 1` reste bit-pour-bit le chemin
  pré-r459** : un seul attachement, une seule image, aucune image de
  résolution créée, mêmes appels Vulkan — zéro risque pour le chemin
  déjà vérifié.

## Établi — vérifié sans régression, élimine son propre motif de rejet

`ctest` natif complet : **11/11** (dont `ac6_native_xenos_tests`,
34,6 s, aucune régression). Relancé contre l'ISO réelle avec
`AC6_NATIVE_VD_TRACE=1 AC6_NATIVE_CAPTURE=1` :
```
ac6recomp: presented_frames=5 state=2
ac6recomp: capture pixels=921600 non_black=0 distinct_colors=1
```
**Le motif de rejet « MSAA render targets are not qualified this
cycle » a disparu (0 occurrence)** — le correctif fonctionne comme
prévu, aucun crash, aucune régression de `presented_frames` (toujours
5, conforme à la ligne de base). Les 9 replis restants (même nombre
qu'aux cycles précédents) portent maintenant deux motifs SANS RAPPORT
avec MSAA : 8× « active shader is not pinned : no pinned variant
matches this draw state » (le trou de couverture du registre épinglé
déjà caractérisé par r456, nécessite l'oracle) et 1× (nouveau,
non-MSAA) « partial render-target write masks are not qualified this
cycle » (une garde préexistante, non touchée ce cycle, non
investiguée).

## Non établi — délibérément, honnêtement signalé

**La géométrie de tuile EDRAM (`pitch_pixels`/`base_tiles` → origine/
dimensions d'image) n'a PAS été ajustée pour `sample_count > 1`** —
elle reste le calcul purement en pixels hérité du chemin 1×. Ce
cycle a délibérément refusé de deviner si le champ de pitch de
`RB_SURFACE_INFO` intègre déjà le nombre d'échantillons (donc aucun
ajustement hôte n'est nécessaire) ou s'il en faudrait un. La seule
preuve indirecte disponible dans ce dépôt est le chemin de validation
structurelle pure de `VulkanBackend::submit()` (pas
`PinnedShaderRuntime`), où `source_bytes` d'un `ResolvePacket` est
calculé comme `width * height * sample_count * 4` — linéaire en
`sample_count` pour une largeur/hauteur pixel FIXE — ce qui a motivé
de qualifier MSAA ici plutôt que de le refuser indéfiniment, mais ne
prouve rien sur la disposition interne des tuiles EDRAM elles-mêmes.
Deviner cette disposition (répartition 2×2, 4×1 ou 1×4 des échantillons
par tuile) sans contrôle serait exactement le type de « règle
plausible sans contrôle » que ce dépôt refuse par discipline (cycles
1111/1113). **Ceci est falsifiable une fois du contenu MSAA réel
rendu** (une image systématiquement décalée ou déchirée en serait le
symptôme) — mais aucun tirage réel de ce run n'a atteint cette
géométrie (tous les tirages qui auraient pu être MSAA sont encore
bloqués par le trou de couverture du registre épinglé, motif
sans rapport).

## Décisions prises

- Résolution explicite (`vkCmdResolveImage` à la présentation) plutôt
  qu'automatique (`pResolveAttachments` par passe) — la seule option
  compatible avec le modèle d'accumulation multi-tirages déjà en place
  pour le cas 1×, sans quoi les échantillons intermédiaires seraient
  perdus entre tirages.
- Ne PAS ajuster la géométrie de tuile EDRAM pour `sample_count > 1` —
  aucune preuve citable dans ce dépôt (retail ou oracle) pour la
  disposition exacte des échantillons par tuile ; signalé explicitement
  plutôt que deviné.
- Toujours ne pas committer — `native_vulkan_backend.{h,cpp}` restent
  les fichiers les plus entremêlés avec l'arriéré déjà catalogué.

## Gate

**Aucune source committée ce cycle** — l'implémentation reste
appliquée localement, vérifiée (build + `ctest` 11/11 + lancement réel
avec trace + capture), non committée.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées — aucune source de production committée ce cycle)
`ctest` racine inchangé (aucune source de production committée ce
cycle).

## Named for r460

Deux pistes distinctes, ni l'une ni l'autre bloquante : (1) le motif
de rejet du registre épinglé (couverture des 271 variantes,
caractérisé par r456, maintenant le SEUL motif MSAA-adjacent restant
sur les tirages observés) nécessite probablement une session oracle —
décision utilisateur, pas à prendre seul ; (2) le nouveau motif
« partial render-target write masks are not qualified this cycle »
observé une fois ce cycle, sans rapport avec MSAA, non investigué —
pourrait être un autre correctif ciblé ou un vrai trou de couverture,
à déterminer. Reste ouvert sinon : une fois le câblage jugé mûr,
reconsidérer le committage groupé de l'arriéré
(r433/r434/r438/r454-r459) ; mise à jour de
`tools/prepare.py`/`tools/build.py` pour le chemin ISO par défaut
(confort, pas une nécessité).

## Files

Aucun artefact gitignoré nouveau (journal de vérification sous
`/fastdata/tmp/...scratchpad/`, non conservé).

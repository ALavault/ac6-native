# AC6 retail NTSC-U/J — r461 — un scissor démesuré (idiome matériel « pas de découpage supplémentaire ») est maintenant borné à la surface EDRAM réelle au lieu d'être rejeté — élimine son propre motif de rejet, révèle une garde `clip_disable` en aval jamais atteinte auparavant

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r460, piste 2 : le motif de rejet apparu ce cycle-là
« render target region exceeds the bounded size », sans rapport avec
le masque d'écriture, non investigué.

## Établi — un vrai diagnostic en direct, pas une supposition

`derive_edram_render_target()` rejetait toute région de scissor
(`PA_SC_SCREEN_SCISSOR_TL`/`BR`) dépassant `kMaxEdramRegionWidth`/
`kMaxEdramRegionHeight` (4096×4096, une limite déjà existante). Une
trace de diagnostic locale a montré la vraie valeur rencontrée :
```
r461 probe: scissor_tl=0x00000000 scissor_br=0x20002000
            region=(0,0)-(8192,8192) size=8192x8192
```
`0x2000` = 8192 sur les deux axes — un rectangle bien plus grand que
toute surface de rendu réelle de ce titre. C'est l'idiome matériel
standard Xenos/GPU pour « pas de découpage scissor supplémentaire » :
le matériel réel borne de toute façon la rastérisation aux dimensions
réelles de la cible de rendu, donc un scissor nominal plus grand que
la surface est inoffensif, pas une erreur — contrairement à la
question de géométrie de tuile sous MSAA (r459), ceci est le
comportement standard, documenté, d'un test de scissor GPU, pas une
supposition sur une disposition mémoire non vérifiée.

## Correctif appliqué et vérifié (local, non committé — même statut que r454-r460)

Le calcul de la géométrie de tuile EDRAM (`pitch_tiles`/`image_width`/
`image_height`/`origin_x`/`origin_y`) est déplacé AVANT le calcul de la
région de scissor (il ne dépendait déjà que de `pitch_pixels`/
`base_tiles`, pas de la région). Le scissor est ensuite **borné**
(`std::min`) à la surface réelle déjà calculée
(`image_width - origin_x`, `image_height - origin_y`) au lieu d'être
rejeté par une constante arbitraire. Réutilise exactement la même
géométrie déjà en place pour la vérification finale existante
(`origin_x + region_width > image_width`, conservée intacte comme
filet de sécurité). Un scissor qui se borne à une région vide (origine
déjà hors surface) échoue toujours fermé, avec un nouveau message
dédié plutôt que de dessiner silencieusement rien.

## Établi — vérifié sans régression, élimine son propre motif de rejet

`ctest` natif complet : **11/11** (dont `ac6_native_xenos_tests`,
aucune régression). Relancé contre l'ISO réelle avec trace :
```
9 × « no pinned variant matches this draw state » (registre épinglé, inchangé en nature)
1 × « clip_disable viewport path not qualified this cycle » (NOUVEAU, jamais atteint avant)
presented_frames=5 state=2 (inchangé)
capture pixels=921600 non_black=0 distinct_colors=1 (inchangé)
```
**Le motif « render target region exceeds the bounded size » a
disparu (0 occurrence)** — le tirage à scissor démesuré n'est plus
rejeté sur ce critère et progresse maintenant plus loin dans le
pipeline, où il atteint soit le trou de couverture du registre épinglé
déjà connu, soit une garde `clip_disable` (ligne 2771) **jamais
atteinte auparavant** puisque le rejet de scissor court-circuitait
systématiquement avant. Le compte total de replis varie d'un run à
l'autre (bruit de minutage du sondage déjà observé aux cycles
précédents, 8-9 typiquement) ; ce qui compte ici est l'ABSENCE du
motif ciblé et l'ABSENCE de toute régression (`presented_frames`,
capture, `ctest` tous inchangés).

## Non établi

- **La garde `clip_disable viewport path not qualified this cycle`**
  (ligne 2771) — nouvellement atteinte, jamais investiguée
  jusqu'ici puisqu'aucun tirage ne l'atteignait avant ce correctif.
- **Si d'autres tirages avaient un scissor démesuré de façon
  différente** (pas exactement 8192×8192) — seule cette valeur précise
  a été observée en direct ce cycle.

## Décisions prises

- Corriger par bornage plutôt que par rejet — l'idiome de scissor
  démesuré est un comportement matériel standard et bien compris,
  contrairement à la question de disposition d'échantillons EDRAM sous
  MSAA que r459 avait explicitement refusé de deviner.
- Réutiliser la géométrie de tuile déjà calculée pour le bornage
  plutôt que d'introduire une nouvelle formule — zéro nouvelle
  supposition non vérifiée.
- Toujours ne pas committer — `native_vulkan_backend.cpp` reste
  entremêlé avec l'arriéré déjà catalogué.

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

## Named for r462

Deux pistes, ni l'une ni l'autre bloquante : (1) le motif de rejet du
registre épinglé (couverture des 271 variantes, r456) nécessite
probablement une session oracle — décision utilisateur ; (2) la garde
`clip_disable viewport path not qualified this cycle` (ligne 2771),
nouvellement atteignable, jamais investiguée — pourrait être un autre
correctif ciblé sans oracle, sur le même modèle que r457/r460/r461, ou
un vrai trou de couverture. Reste ouvert sinon : une fois le câblage
jugé mûr, reconsidérer le committage groupé de l'arriéré
(r433/r434/r438/r454-r461) ; mise à jour de
`tools/prepare.py`/`tools/build.py` pour le chemin ISO par défaut
(confort, pas une nécessité).

## Files

Aucun artefact gitignoré nouveau (journal de vérification sous
`/fastdata/tmp/...scratchpad/`, non conservé).

# AC6 retail NTSC-U/J — r454 — `PinnedShaderRuntime` câblée dans les chemins réels `VdSwap` et de vidage d'anneau — présents réels confirmés (`present_count` progresse via le vrai moteur, pas juste l'effacement), une régression trouvée et corrigée en cours de cycle (repli sur validation structurelle pour les nuanceurs non épinglés), un vrai défaut de synchronisation de diagnostic trouvé et NON corrigé — NON committé, entièrement entremêlé avec l'arriéré déjà catalogué

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r453 : reprendre le fil du contenu visuel réel
(`PinnedShaderRuntime` jamais branché au chemin `VdSwap`), maintenant
justifié par la richesse de contenu confirmée (86 tirages réels par
lancement contre le vrai ISO).

## Correctif appliqué (local, non committé)

Quatre fichiers modifiés, tous portant déjà un arriéré préexistant
important (déjà catalogué par r433) :

- `native/include/ac6/native_guest_vd.h` : ajout d'un membre non
  possédant `PinnedShaderRuntime* pinned_{}` et d'une méthode
  `bind_pinned(PinnedShaderRuntime*)`, sur le même modèle que
  `bind_offscreen`.
- `native/src/native_guest_vd.cpp` :
  - `drain_locked()` : quand `pinned_` est lié et valide, synchronise
    toute la mémoire invité dans le SSBO du moteur épinglé (copie
    intégrale bornée par `shared_memory_dwords()`, 512 Mio — r438,
    approche naïve non optimisée) puis appelle
    `pinned_->execute_frame(...)` au lieu de
    `backend_->submit(...)` (validation seule).
  - `publish_write_address()` (le chemin `VdSwap` réel, r430) : appelle
    `pinned_->execute_frame(...)` avec un `PresentPacket` construit
    localement au lieu de `backend_->present_to_offscreen(...)`
    (simple effacement) quand `pinned_` est lié.
- `native/include/ac6/native_runtime.h` / `native/src/native_runtime.cpp` :
  ajout d'un membre `std::unique_ptr<PinnedShaderRuntime>
  pinned_runtime_`, construit dans `bind_guest_vd()` juste après
  `offscreen_device_`/`offscreen_target_` (même contrat
  nul-si-pas-de-Vulkan), lié via `bind_pinned()`.

## Établi — vérifié en direct, régression trouvée et corrigée dans le même cycle

**Premier essai** : lancé contre l'ISO réelle (r452), un tirage réel a
échoué le rejet de `draw_pinned` (« active shader is not pinned : no
pinned variant matches this draw state » — attendu, le registre
épinglé compte 271 traductions issues de l'oracle, r255, pas
exhaustif). **Ce rejet, non géré, faisait échouer TOUT le lot décodé**
(pas seulement le rendu réel) — contrairement à
`VulkanBackend::submit()` qui ne fait QUE de la validation
structurelle et n'échoue jamais sur une capacité de rendu manquante.
Conséquence mesurée : `presented_frames=0 state=1` (contre la ligne de
base saine `5`/`2`) — **une vraie régression par rapport au
comportement d'avant ce correctif**, capturée avant tout commit.

**Corrigé dans le même cycle** : un échec de `execute_frame()` ne fait
plus échouer le lot — il retombe sur `backend_->submit(...)` (pure
validation structurelle) pour CE lot, exactement le comportement
d'avant r454, l'anneau invité continue de progresser normalement. Un
drapeau séparé (`pinned_handled_present`, distinct de `use_pinned`)
garde le bloc d'effacement de repli correctement actif quand ce repli
se produit, pour éviter de perdre le présent de ce lot.

**Relancé** : `exit=0`, **5 lignes `vd swap presented` avec
`present_count` progressant 1→5 via le vrai moteur** (pas
l'effacement), **8 replis sur validation structurelle** enregistrés
(des tirages avec un nuanceur non épinglé, gérés proprement sans
faire échouer le lot). `ctest` natif : **11/11** après ce correctif.

## Non établi / trouvé mais NON corrigé — un vrai défaut de synchronisation de diagnostic

`ac6recomp: presented_frames=0 state=1` en fin de fenêtre, **malgré**
`present_count` progressant réellement à 5 dans la trace. Cause :
`NativeRuntime::diagnostics()` (r431) synchronise `presented_frames`
depuis `backend_.present_count()` — **le compteur de
`VulkanBackend`, pas celui de `PinnedShaderRuntime`**. Quand le chemin
épinglé gère le présent (cas normal maintenant), le compteur de
`backend_` n'avance jamais, et le diagnostic affiché reste figé à 0.
**Non corrigé ce cycle** — nécessiterait de faire pointer
`diagnostics()` vers le bon compteur selon quel moteur a réellement
présenté, ou d'unifier les deux compteurs.

## Non établi — le contenu visuel réel n'est pas confirmé à l'œil

Aucune capture d'image ni comparaison de hachage de couleur tentée ce
cycle — seul le comptage de présents et l'absence de rejet/crash sont
vérifiés. Le placeholder d'effacement n'apparaît plus dans le chemin
principal, mais rien ne confirme encore que les pixels produits sont
visuellement corrects.

## Décisions prises

- **Ne pas committer ce cycle.** Les quatre fichiers touchés portent
  déjà un arriéré substantiel (catalogué par r433) ; ce correctif y
  ajoute un changement de comportement réel et significatif
  (acheminement du présent/rendu réel), pas seulement une ligne isolée
  comme les précédents correctifs entremêlés déjà committés (r430,
  r431, r438). Le défaut de synchronisation de diagnostic trouvé et
  non corrigé, et l'absence de vérification visuelle, rendent ce
  travail explicitement « premier jet fonctionnel, pas encore prêt » —
  cohérent avec la décision déjà prise pour le redimensionnement SSBO
  de r438.
- Corriger la régression du repli AVANT de considérer quoi que ce soit
  d'autre — une régression mesurée en cours de cycle est prioritaire
  sur la poursuite de nouvelles fonctionnalités.
- Documenter le défaut de synchronisation de diagnostic honnêtement
  plutôt que de le corriger à la hâte en fin de cycle sans le temps de
  le vérifier correctement.

## Gate

**Aucune source committée ce cycle** — les quatre fichiers modifiés
restent appliqués localement, vérifiés (build + `ctest` 11/11 + deux
lancements réels contre l'ISO), non committés.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus, inchangées — aucune source de
production committée ce cycle)
`ctest` racine inchangé (aucune source de production committée ce
cycle).

## Named for r455

Deux pistes, ni l'une ni l'autre bloquante : (1) corriger le défaut
de synchronisation de `presented_frames` (faire pointer
`diagnostics()` vers le bon compteur, ou unifier `backend_`/`pinned_`)
et capturer une image réelle pour vérifier visuellement le rendu ;
(2) une fois ces deux points réglés et le travail jugé suffisamment
mûr, reconsidérer le committage groupé de l'arriéré
`native_vulkan_backend.cpp`/`native_guest_vd.cpp`/`native_runtime.*`
(r433/r434/r438, maintenant encore plus entremêlé avec ce correctif).
Reste ouvert sinon : mise à jour de `tools/prepare.py`/`tools/build.py`
pour le chemin ISO par défaut (r452/r453, confort, pas une nécessité).

## Files

Aucun artefact gitignoré nouveau (journaux de vérification sous
`/fastdata/tmp/...scratchpad/`, non conservés).

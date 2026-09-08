# AC6 retail NTSC-U/J — r434 — cause précise trouvée pour « le contenu visuel réel reste un effacement placeholder » : `PinnedShaderRuntime` (le vrai moteur de rendu, testé r256-r270) n'est JAMAIS appelé depuis le chemin `VdSwap` en direct — et son contrat mémoire partagée (64 MiB, adressage direct) est incompatible tel quel avec de vraies adresses invité 32 bits, pas seulement non câblé

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Ligne ouverte la moins chère nommée depuis r431/r432/r433, non
bloquée par la décision de committage de l'arriéré (r433, r434
n'a rien committé de nouveau à ce sujet) : le contenu visuel réel des
présents (`present_to_offscreen` n'effectue toujours qu'un effacement
de couleur, jamais un vrai rendu).

## Établi

### `PinnedShaderRuntime` est un moteur de rendu complet, déjà écrit et testé (r256-r270)

`native/src/native_vulkan_backend.cpp` contient une classe
`PinnedShaderRuntime` complète : `draw_pinned()` (tirages indexés
réels, listes de rectangles r265, redémarrages de primitive r263,
échantillonnage de texture réel r258), `derive_edram_render_target()`,
`resolve_edram_to_target()` (résolution EDRAM→image r257), et
`execute_frame(VulkanOffscreenTarget&, const XenosState&,
std::span<const XenosCommand>)` — la méthode d'entrée : `DrawPacket`
→ `draw_pinned()` réel, `PresentPacket` → résolution EDRAM réelle
(pas un effacement) si un render target EDRAM est actif.

### Ce moteur n'est appelé QUE depuis les tests, jamais depuis le chemin d'exécution réel

```
grep -rln "PinnedShaderRuntime" native/ reconstruction/
  → native/include/ac6/native_vulkan_backend.h
  → native/src/native_vulkan_backend.cpp
  → native/tests/native_xenos_tests.cpp
```

Ni `native_runtime.cpp`, ni `native_guest_vd.cpp`, ni
`ac6recomp_main.cpp` ne le référencent. Le chemin réel du drain de
l'anneau (`native_guest_vd.cpp`, fonctions autour des lignes 243-403)
appelle exclusivement `backend_->submit(*state_, commands)`
(`VulkanBackend::submit` — validation et comptage seuls, aucun
tirage) puis, sur `PresentPacket`/le déclenchement direct de r430,
`backend_->present_to_offscreen(...)` (`VulkanBackend`, PAS
`PinnedShaderRuntime` — un simple `target.clear(r, g, b, a)`, avec le
commentaire déjà présent dans le code : « Placeholder clear color
until resolve-to-image lands »). **Le moteur de rendu réel existe,
compile, passe ses tests — et n'est simplement jamais instancié sur
le chemin que le jeu emprunte réellement.**

### Le contrat mémoire partagée de `PinnedShaderRuntime` n'est pas seulement non câblé — il est dimensionné pour des tests synthétiques, pas pour de vraies adresses invité

`PinnedShaderRuntime::write_shared_memory(dword_address, bytes)`
écrit dans un unique SSBO Vulkan de **64 MiB** alloué au
constructeur (`shared_memory_dwords_ = 64*1024*1024/4`), avec un
commentaire explicite : « the size is an explicit contract, not a
guess ». `draw_pinned()` utilise `draw.vertex_address` /
`draw.index_address` **directement comme décalage d'octet** dans ce
SSBO de 64 MiB (`byte_offset = draw.index_address`), borné et refusé
au-delà (« fail closed »). Dans les tests (`native_xenos_tests.cpp`),
les adresses invité synthétiques utilisées sont volontairement
petites (`0x1000`, `0x2000`, `0x100000`…) — compatibles avec ce
décalage direct par construction du test, pas parce que ce sont de
vraies adresses Xbox 360.

**Sur le chemin réel**, les adresses des `DrawPacket` proviennent des
registres Xenos réels du flux PM4 du jeu (`kRegShaderConstantFetch000`
etc., déjà décodés ailleurs dans ce même fichier) — de vraies adresses
32 bits pouvant se situer n'importe où dans les 512 Mio de RAM
physique Xbox 360, pas seulement sous les 64 Mio du SSBO synthétique.
**Router le chemin réel vers `execute_frame` sans résoudre ce
désaccord d'adressage échouerait silencieusement en `fail closed` sur
la quasi-totalité des tirages réels** (adresses hors de la fenêtre de
64 Mio), ou pire, écrirait au mauvais décalage si un mappage naïf
adresse-invité → décalage-SSBO était supposé sans preuve.

## Non établi

- **La plage réelle des adresses invité utilisées par les tirages du
  jeu** — non capturée en direct ce cycle (nécessiterait une
  instrumentation gdb similaire à celle de r426/r428/r429 sur le
  décodeur PM4 réel du chemin `drain_locked`/`submit`, pour lire les
  vrais `vertex_address`/`index_address`/adresses de fetch de texture
  qu'un tirage retail produit).
- **Si le SSBO devrait couvrir toute la RAM physique (512 Mio) avec
  adressage direct par adresse invité**, ou si un mappage
  fenêtré/paginé serait nécessaire pour rester dans un budget mémoire
  GPU raisonnable — dépend directement de la plage réelle non encore
  mesurée.
- **Si l'exécution réelle de `execute_frame` sur le chemin retail
  produirait un rendu visuellement correct** une fois l'adressage
  résolu — aucune vérification visuelle possible sans cette étape
  préalable.

## Décisions prises

- **Ne pas router `execute_frame` vers le chemin `VdSwap` réel ce
  cycle.** Le faire sans mesurer d'abord la plage réelle d'adresses
  invité des tirages serait « une règle plausible sans contrôle »
  (refusée par la discipline de ce dépôt, cf. r1111/r1113) : le
  résultat le plus probable serait soit un échec silencieux
  généralisé (`fail closed` hors de la fenêtre de 64 Mio), soit un
  rendu incorrect si un mappage d'adresse était deviné sans preuve.
- **Documenter précisément la cause plutôt que de tenter un
  câblage non vérifié** — la ligne ouverte « contenu visuel réel »
  nommée depuis r431 n'était pas juste « pas encore fait », elle a
  maintenant une cause racine concrète et un obstacle technique
  identifié (le désaccord d'adressage), ce qui change la nature du
  travail restant : mesurer avant de câbler, pas câbler puis
  découvrir l'échec.

## Gate

Aucune source de production éditée ce cycle (investigation en lecture
seule : `native_vulkan_backend.cpp`, `native_vulkan_backend.h`,
`native_guest_vd.cpp`, `native_runtime.h`, `native_xenos_tests.cpp`).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus, inchangées depuis r433 — aucune
source affectée)
`ctest` racine inchangé depuis r432/r433 (aucune source modifiée).

## Named for r435

Capturer en direct (gdb, sur le modèle de r426/r428/r429) les
adresses invité réelles (`vertex_address`, `index_address`, adresses
de fetch de texture) d'un tirage réel `DrawPacket` sur le chemin
`drain_locked`/`submit` du jeu en cours d'exécution — cela tranchera
si la fenêtre actuelle de 64 Mio à adressage direct peut fonctionner
telle quelle (si les adresses réelles s'y trouvent par chance) ou si
`PinnedShaderRuntime` a besoin d'une refonte de son contrat mémoire
partagée (fenêtre plus large et/ou remappage adresse-invité →
décalage-SSBO) avant tout câblage vers le chemin `VdSwap` réel.

## Files

Aucun artefact gitignoré nouveau.

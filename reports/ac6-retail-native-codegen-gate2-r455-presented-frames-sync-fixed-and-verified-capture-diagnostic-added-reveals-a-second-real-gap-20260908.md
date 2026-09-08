# AC6 retail NTSC-U/J — r455 — le défaut de synchronisation `presented_frames` de r454 est corrigé et vérifié (`5/2`, conforme à la ligne de base) ; un diagnostic de capture ajouté révèle un SECOND vrai défaut : sur ce lancement, AUCUN tirage n'a jamais correspondu à un nuanceur épinglé, donc l'image cible n'a jamais été touchée du tout (ni effacée, ni résolue) — régression réelle par rapport à l'ancien chemin qui effaçait toujours

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r454 : corriger le défaut de synchronisation de
`presented_frames` et capturer une image réelle pour vérification
visuelle.

## Correctif appliqué et vérifié (local, non committé — même statut que r454)

`native/include/ac6/native_runtime.h`, `diagnostics()` : additionne
maintenant `backend_.present_count()` **et**
`pinned_runtime_->present_count()` (les deux chemins sont additifs,
jamais superposés sur le même paquet, garanti par les gardes
`pinned_handled_present`/`use_pinned` déjà en place dans
`native_guest_vd.cpp`).

**Vérifié en direct** : `ctest` natif 11/11, puis lancement contre
l'ISO réelle : `ac6recomp: presented_frames=5 state=2` — **conforme
à la ligne de base saine**, corrigeant le blocage à 0 observé par
r454.

## Diagnostic de capture ajouté (local, non committé)

`NativeRuntime::readback_offscreen_pixels()`/`offscreen_error()`
(nouveaux accesseurs, exposent `VulkanOffscreenTarget::readback()`/
`error()` déjà existants) + un bloc dans `ac6recomp_main.cpp` gardé
par `AC6_NATIVE_CAPTURE=1` : compte les pixels non noirs et les
couleurs distinctes en fin de fenêtre de sondage — pas d'écriture de
fichier, pas de conversion PNG, juste assez pour distinguer « encore
un simple effacement » de « du vrai contenu a été dessiné ».

## Établi — la capture a révélé un second vrai défaut, pas juste confirmé le premier

```
ac6recomp: presented_frames=5 state=2
ac6recomp: capture failed: has_offscreen_present_target=1
           error=offscreen image is not in a blittable layout
```

`VulkanOffscreenTarget::ensure_transfer_src()` échoue ainsi
uniquement quand `layout_` n'est NI `TRANSFER_SRC_OPTIMAL` NI
`TRANSFER_DST_OPTIMAL` — c'est-à-dire encore à son état initial
`UNDEFINED`, jamais transitionné par quoi que ce soit. Recoupé avec
r454 : sur ce même run, **les 8 tentatives de tirage réel sont
TOUTES tombées en repli** (aucun nuanceur épinglé ne correspondait à
l'état de dessin observé) — `edram_rt_valid_` de `PinnedShaderRuntime`
n'est donc jamais devenu vrai, et l'appel direct de présent de
`publish_write_address` (le chemin `VdSwap`, r430/r454) vers
`execute_frame()` avec un seul `PresentPacket` **ne fait alors
strictement rien sur l'image** (pas de résolution EDRAM car aucune
cible active, et — contrairement à l'ancien chemin `backend_->
present_to_offscreen()` qui appelait toujours `clear()` — aucun
effacement de secours n'est déclenché non plus).

**Ceci est une régression réelle par rapport à l'ancien
comportement** : avant r454, chaque présent garantissait AU MOINS une
image définie (effacée en noir). Après r454, si aucun tirage épinglé
ne réussit jamais sur toute la session, l'image reste indéfinie en
permanence — pas un crash, pas un problème pour le compteur
`presented_frames` (qui avance correctement), mais un vrai recul pour
toute lecture/capture de l'image elle-même.

## Non établi

- **Si un contenu visuel réel a DÉJÀ été rendu au moins une fois** sur
  d'autres lancements (le taux de correspondance du registre épinglé
  peut varier selon quels nuanceurs le jeu sollicite à quel instant) —
  non revérifié sur plusieurs lancements ce cycle.
- **Le contenu réel des pixels** quand la capture réussira — reste à
  vérifier une fois le défaut ci-dessus corrigé.

## Décisions prises

- Corriger le défaut de synchronisation immédiatement (petit, isolé,
  vérifié) mais NE PAS tenter de corriger le défaut d'image
  indéfinie dans ce même cycle — sa correction correcte nécessite de
  décider comment garantir une image définie sans réintroduire le
  double-effacement déjà corrigé une fois par r454, ce qui mérite
  d'être fait avec autant de soin que le premier correctif, pas
  précipité en fin de cycle.
- Garder l'outillage de capture (non committé, comme le reste) même
  si sa toute première utilisation a révélé un échec plutôt qu'un
  succès — c'est exactement le rôle prévu : détecter ce genre de
  régression avant tout commit.
- Toujours ne pas committer — l'entremêlement avec l'arriéré déjà
  catalogué (r433) et l'existence d'un défaut connu non corrigé
  restent des raisons suffisantes, cohérent avec r454.

## Gate

**Aucune source committée ce cycle** — les correctifs et
l'instrumentation restent appliqués localement, vérifiés (build +
`ctest` 11/11 + lancements réels), non committés.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus, inchangées — aucune source de
production committée ce cycle)
`ctest` racine inchangé (aucune source de production committée ce
cycle).

## Named for r456

Garantir une image définie sur le chemin de présent direct
(`publish_write_address`) même quand aucun tirage épinglé n'a réussi
sur la session — sans réintroduire le double-effacement/double-
présent déjà corrigé par r432/r454 (probablement : appeler `clear()`
en secours uniquement si `edram_rt_valid_` de `pinned_` n'a jamais
été vrai, une nouvelle information que `PinnedShaderRuntime` ne
publie pas encore). Puis relancer la capture pour vérifier
visuellement du vrai contenu. Reste ouvert sinon : une fois mûr,
reconsidérer le committage groupé de l'arriéré (r433/r434/r438/
r454/r455) ; mise à jour de `tools/prepare.py`/`tools/build.py` pour
le chemin ISO par défaut (r452/r453, confort, pas une nécessité).

## Files

Aucun artefact gitignoré nouveau (journaux de vérification sous
`/fastdata/tmp/...scratchpad/`, non conservés).

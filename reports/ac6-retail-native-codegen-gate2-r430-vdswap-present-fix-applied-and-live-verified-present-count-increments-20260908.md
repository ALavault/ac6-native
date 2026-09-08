# AC6 retail NTSC-U/J — r430 — correctif appliqué et vérifié en direct : `VdSwap` déclenche désormais un vrai présent, `present_count` progresse 1→5 — le plan d'injection PM4 de r429 est abandonné (mauvais modèle), remplacé par un appel direct beaucoup plus sûr

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r429 (nommé pour r430) : confirmer l'emplacement exact
d'écriture dans l'anneau avant d'appliquer le correctif de synthèse
PM4 conçu par r429.

## Correction majeure du modèle avant d'appliquer quoi que ce soit

En relisant `discover_write_index_locked` (`native/src/
native_guest_vd.cpp:198-233`, déjà lu par r418/r425/r426 mais pas
pour CETTE question précise) pour répondre à la question de r429 :
**cette fonction lit l'index d'écriture depuis le champ `+10952` de
l'« objet producteur » (l'objet dispositif du jeu, `r31` dans
`sub_821F03B0`) — PAS depuis le curseur que `VdSwap` reçoit en
argument (`ctx.r3`, `guest_address`).** Dans le chemin retail réel
(`readback_ != 0`), `guest_address` n'est utilisé QUE pour la trace de
diagnostic — il n'intervient dans AUCUN calcul d'indexation de
l'anneau.

**Ceci invalide directement le plan de r429** : il n'existe pas de «
bon emplacement d'anneau » où injecter un paquet `XE_SWAP` synthétisé
à partir du curseur de `VdSwap`, parce que ce curseur n'alimente pas
le mécanisme de découverte/drainage de l'anneau. Le flux PM4
(tirages, attentes, écritures d'événement — tout ce que r418/r425/r426
ont décodé) provient exclusivement de la comptabilité PROPRE au jeu
sur son objet dispositif (`+48`/`+10896`/`+10952`), gérée par le code
de rendu ordinaire (comme le paquet `EventWriteShd` décodé par r428) —
un mécanisme entièrement SÉPARÉ de `VdSwap`.

**Ceci est cohérent avec le matériel Xbox 360 réel** : `VdSwap` est un
appel noyau à part entière qui programme directement le contrôleur
d'affichage (adresse de tampon frontal, dimensions) — ce n'est PAS une
commande PM4 que le jeu place dans l'anneau de rendu. L'anneau porte
les commandes qui PRODUISENT le contenu du tampon frontal ; `VdSwap`
notifie séparément le matériel de PRÉSENTER ce tampon.

## Correctif appliqué

`native/src/native_guest_vd.cpp`, dans
`NativeGuestVdService::publish_write_address` (le seul point d'entrée
de `VdSwap`, déjà établi par r425) : appel direct à
`backend_->present_to_offscreen(...)`, sans passer par l'anneau PM4 ni
écrire de mémoire invitée :

```c++
if (base_ != nullptr && base == base_ && backend_ != nullptr &&
    present_target_ != nullptr) {
  const PresentPacket packet{0u, present_target_->width(),
                             present_target_->height(), 0u};
  if (!backend_->present_to_offscreen(*present_target_, packet, 0.0f, 0.0f,
                                      0.0f, 1.0f)) {
    trace("vd swap present failed %ux%u: %s", ...);
  } else {
    trace("vd swap presented %ux%u present_count=%llu", ...);
  }
}
```

Les dimensions viennent de `present_target_->width()/height()`,
interrogées en direct (pas codées en dur) — la même cible déjà
confirmée à `1280×720` par r429. Aucune écriture de mémoire invitée,
donc aucun risque de corrompre un paquet PM4 déjà valide (le risque
identifié par r429 pour justifier de ne pas appliquer son plan).

## Établi — vérifié en direct

- Reconstruction (`ninja ac6recomp`) : succès.
- **Lancement avec `AC6_NATIVE_VD_TRACE=1` (fenêtre `25000ms`)** :
  ```
  vd swap presented 1280x720 present_count=1
  vd swap presented 1280x720 present_count=2
  vd swap presented 1280x720 present_count=3
  vd swap presented 1280x720 present_count=4
  vd swap presented 1280x720 present_count=5
  ```
  **`present_count` progresse à chaque appel `VdSwap`, exactement 5
  fois — un pour chacun des 5 appels déjà comptés par r425/r428.**
- **`ac6recomp: presented_frames=0` s'affiche TOUJOURS** en fin de
  run — ATTENDU, pas un échec du correctif : `diagnostics_.
  presented_frames` n'est mis à jour QUE dans
  `NativeRuntime::submit_ring()` (code mort, r292/r418) ; le chemin
  réel (`NativeGuestVdService::drain_locked`/maintenant
  `publish_write_address`) écrit directement dans le compteur PROPRE
  de `VulkanBackend` (`backend_.present_count()`), jamais resynchronisé
  vers `diagnostics_.presented_frames`. **Un défaut séparé et déjà
  identifié (r292), pas résolu par ce correctif et hors de son
  périmètre.**
- `ctest` natif : **11/11** réussis (`16.42s`, `ac6_native_xenos_tests`
  inclus).
- Lancement autonome de contrôle (sans gdb, sans trace) : `exit=0`,
  `elapsed=26s` — sortie propre, conforme à la leçon méthodologique de
  r419.
- Gates du dépôt et `ctest` racine : tous verts, même échec préexistant
  sans rapport (`ac6-cpp-complexity`).

## Ce que ceci établit

**La chaîne r418-r430 est fonctionnellement close pour ce qui concerne
le mécanisme de présentation lui-même** : `VdSwap` déclenche
maintenant un vrai présent Vulkan à chaque appel, avec les bonnes
dimensions, sans risque de corruption de l'anneau. Le compteur
`present_count()` du backend — la source de vérité réelle — progresse
correctement. **Reste un défaut de câblage séparé et déjà nommé
(r292)** entre ce compteur et le diagnostic affiché
`presented_frames`, qui n'était jamais dans le périmètre de ce
correctif.

## Non établi

- **Le contenu visuel du présent** — `present_to_offscreen` fait
  actuellement un simple effacement de couleur (« Placeholder clear
  color until resolve-to-image lands: opaque black », commentaire déjà
  présent, non ajouté ce cycle) — pas encore les vrais pixels du jeu.
  Sans rapport avec ce correctif : le mécanisme de déclenchement du
  présent est maintenant correct, le contenu qu'il présente est un
  chantier séparé, déjà nommé ailleurs dans le projet.
- **Reconnecter `diagnostics_.presented_frames` à `backend_.
  present_count()`** — non fait ce cycle, `submit_ring()` reste du
  code mort inchangé.

## Décisions prises

- Abandonner le plan de synthèse/injection PM4 de r429 dès que la
  relecture de `discover_write_index_locked` a montré qu'il reposait
  sur un mauvais modèle — corriger sa propre conception avant de
  l'appliquer plutôt que de s'y tenir par inertie (précédent CLAUDE.md :
  corriger soi-même, par nom et numéro de cycle).
- Vérifier le succès via `backend_->present_count()` directement (la
  trace `AC6_NATIVE_VD_TRACE`) plutôt que via `presented_frames` de
  `ac6recomp`, sachant CE dernier reste cassé par un défaut séparé —
  mesurer ce que le correctif est censé changer, pas un diagnostic
  qu'il ne touche pas.
- Ne pas reconnecter `diagnostics_.presented_frames` dans ce même
  cycle — un bug à la fois (déjà nommé séparément par r292/r418).

## Divulgation de portée du commit — même situation que r423, signalée par avance cette fois

`native_guest_vd.cpp`/`.h` portent un arriéré non committé
préexistant (`r282`, `r292`-`r296`) — la même instrumentation
`present_target_`/`bind_offscreen()`/les traces de diagnostic déjà
lues et citées tout au long de cette chaîne (r418, r425, r426, r428).
**Contrairement à r423**, ce correctif ne peut PAS être isolé
proprement de cet arriéré : `present_target_`/`bind_offscreen`
n'existent PAS dans la version committée (`HEAD`) de ces fichiers — le
correctif de ce cycle en dépend directement pour compiler. Committer
uniquement mes lignes casserait la compilation. **Décision** :
committer l'état complet des deux fichiers, en le disant explicitement
ici plutôt que de laisser le message de commit sous-décrire la
portée (leçon directement tirée de r423, appliquée par avance cette
fois plutôt que corrigée après coup).

## Gate

**Sources de production éditées ce cycle** (avec divulgation
ci-dessus) :
`recompilation/ace-combat-6-retail/native/src/native_guest_vd.cpp`,
`recompilation/ace-combat-6-retail/native/include/ac6/native_guest_vd.h`.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus)
`ctest` (racine et natif) relancés en entier.

## Named for r431

Reconnecter `diagnostics_.presented_frames` à `backend_.
present_count()` sur le chemin réel (`NativeGuestVdService`), pas
seulement dans le code mort `submit_ring()` — pour que le diagnostic
affiché par `ac6recomp` reflète enfin ce que ce cycle vient de
corriger.

## Files

Aucun artefact gitignoré nouveau (les journaux de vérification sont
sous `/tmp`, non conservés).

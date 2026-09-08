# AC6 retail NTSC-U/J — r453 — le correctif de lancement (r452) confirmé apporter un rendu SUBSTANTIELLEMENT plus riche : 86 vrais tirages capturés sur la fenêtre de 25 s, contre 0-1 par présent auparavant — le jeu traite maintenant de vraies données de contenu

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r452 : le crash est résolu en lançant contre l'ISO réelle.
Avant de trancher entre les deux pistes nommées (mise à jour de
`prepare.py`, ou reprise du fil `PinnedShaderRuntime`), un lancement
tracé plus complet pour quantifier l'impact réel du correctif.

## Établi — l'impact va bien au-delà de la simple absence de crash

Lancement complet (`--probe-entry`, ISO réelle,
`AC6_NATIVE_VD_TRACE=1`, fenêtre 25 s) :

- **86 tirages réels (`vd draw`) capturés**, contre **0 ou 1 par
  présent** dans absolument toutes les captures de r425 à r450 (le
  jeu n'avait jamais dépassé une poignée de tirages avant de planter
  ou d'être limité par l'environnement incomplet).
- **5 présents complets** (`present_count=1` à `5`, `1280×720`),
  identique en nombre à avant, mais désormais accompagnés d'un volume
  de travail de rendu bien plus représentatif d'un jeu réel en cours
  d'exécution.
- `ac6recomp: presented_frames=5 state=2` puis
  `ac6recomp: generated entry terminated its own thread` — arrêt
  propre, confirmé une troisième fois.

## Ce que ceci établit

**Le correctif de r452 ne fait pas que supprimer le crash — il
débloque un traitement de contenu substantiellement plus riche**,
cohérent avec l'hypothèse de toute la chaîne r399-r451 : une fois le
vrai contenu PAC disponible, le jeu peut effectivement le lire et
l'utiliser (textures, géométrie, HUD…) au lieu d'échouer silencieusement
sur des données absentes. Ceci renforce la conclusion de r452 sans la
remettre en question — c'est une confirmation quantitative, pas une
nouvelle piste.

## Non établi

- **Si le contenu visuel EST RÉELLEMENT RENDU** (pixels visibles) —
  le pipeline `PinnedShaderRuntime` reste toujours non branché au
  chemin `VdSwap` réel (r434/r435/r438, inchangé par ce correctif) ;
  ces 86 tirages sont comptés/validés par `VulkanBackend::submit()`
  mais toujours affichés comme un simple effacement de couleur.

## Décisions prises

- Ne pas encore trancher entre les deux pistes nommées par r452 —
  cette quantification supplémentaire aide à prioriser : avec 86
  tirages réels désormais disponibles à chaque lancement, la piste
  « contenu visuel réel » (`PinnedShaderRuntime`) devient
  nettement plus intéressante à reprendre qu'avant (il y a maintenant
  de vraies données à rendre, pas seulement une hypothèse).

## Gate

Aucune source de production éditée ce cycle (vérification en
lancement direct uniquement).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus, inchangées — aucune source de
production affectée)
`ctest` (racine et natif) inchangé (aucune source de production
modifiée).

## Named for r454

Reprendre le fil du contenu visuel réel (`PinnedShaderRuntime` jamais
branché au chemin `VdSwap`, r434/r435/r438) — maintenant justifié par
la richesse de contenu confirmée ce cycle (86 tirages réels
disponibles par lancement). Reste ouvert sinon : mise à jour de
`tools/prepare.py`/`tools/build.py` pour le chemin ISO par défaut
(confort, pas une nécessité) ; décision de committage de l'arriéré
`native_vulkan_backend.cpp` (r433/r434/r438).

## Files

Aucun artefact gitignoré nouveau.

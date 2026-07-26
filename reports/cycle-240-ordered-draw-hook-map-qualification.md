# AC6 cycle 240 — qualification de la cartographie des hooks de draw ordonnés

Date : 2026-07-19

## Question

La nouvelle archive ferme-t-elle la jointure retail MATE→shader→état→draw, et
ses pseudo-diffs peuvent-ils être appliqués au checkout AC6Recomp courant ?

## Identité et intégrité

- archive : `ac6_ordered_draw_hook_map_v1.zip` ;
- SHA-256 :
  `5c90f515a187447c4f87a5607596f010e96cd0cff5897e39bdb10bff06c3608d` ;
- XEX PAL qualifié :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- `DATA.TBL` qualifié :
  `82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5` ;
- l'entrée 163 est bien zero-based, à `file+0xA38`, record SHA-256
  `321ca1d2bf25832fd28a75be0ae2b876ba5bfe8162f3bb7c1fdfa366968f5627` ;
- tous les membres déclarés par `SHA256SUMS` passent ;
- le modèle de jointure synthétique passe **10/10** : cardinalité exacte de
  un, rejets zéro/deux, collision, alias MATE, état divergent, ticket sans
  draw, draw tué par visibilité et absence de recherche dans la file.

L'archive ne contient aucun XEX, PAC, cache shader ou autre asset retail.

## Ce que l'archive qualifie

Les points suivants sont qualifiés contre des révisions sources exactes :

- `ShaderDef` dans ReXGlue lors de `PM4_IM_LOAD` et
  `PM4_IM_LOAD_IMMEDIATE` ;
- `DrawEvent` lors du décodage du paquet, puis les preuves d'émission hôte
  D3D12 et Vulkan ;
- `StateDef` immédiatement avant l'émission hôte, après les portes backend ;
- les trois intentions de draw guest aux entrées
  `0x821DEF18`, `0x821DF300` et `0x821DEA48` ;
- la jointure doit utiliser une identité guest et une file ordonnée, jamais un
  timestamp ou un pointeur hôte.

`MaterialBind` n'est pas qualifié. Les couches GPU ont déjà aplati la
sémantique MATE en shader et état. Son point de publication doit encore être
trouvé dans le XEX après résolution technique/passe/permutation et avant les
draws causaux. Le statut reste donc **`needs-xex-instrumentation`** et non
`verified`.

## Recoupement avec le checkout courant

L'archive est bornée à AC6Recomp
`83a785d5c6598ba8c1964a95318595b406e931bb` et ReXGlue
`ec2d07a873cf4c6a4d4bb9d70c8a2fed32ec72`. Le checkout AC6Recomp local est
désormais `c5b089fb6988ac504ba394db611543bda2fb2c96`; l'ancienne révision est un
ancêtre, mais les fichiers ciblés ont changé.

Le checkout courant possède déjà :

- `ShadowState` ;
- `DrawCallRecord` avec séquence, nature du draw et état copié ;
- `CaptureDrawCall` aux trois entrées guest ;
- `FrameCaptureSnapshot` et une file de draws par frame.

Le blob Git courant de `src/d3d_hooks.cpp` est
`3112e594c15913e163b661e0e98e3a6161a5cc67`, différent du blob
`6d02663e5eda3883d2c532d107145c7c7e55e26f` qualifié par l'archive. Les
pseudo-diffs ne sont donc ni appliqués ni décrits comme compilables. Une future
intégration doit enrichir la capture existante, pas créer un second sink.

## Recoupement XEX headless

Le setter nommé `0x82334178` reste un consommateur de paramètres enregistrés,
pas un sélecteur MATE. Ses quatre appelants directs exportés sont
`0x82105aa8`, `0x821748f8`, `0x82175680` et `0x82175ef0`; les trois derniers
appartiennent à une famille d'état global/caméra et ne conservent pas une
identité MATE qualifiée.

Les appels directs vers les feuilles draw sont nombreux et distincts. Le
graphe direct borné actuellement exporté ne fournit aucun ancêtre commun entre
les racines du setter/flush shader examinées et les trois feuilles draw dans
une profondeur de six. Cette absence est une limite d'export, notamment pour
les dispatchs virtuels, et ne prouve pas l'absence d'un `MaterialBind` dans le
XEX.

La prochaine passe statique doit donc partir des producteurs/consommateurs
MATE, suivre les vtables et le contexte guest, puis exiger un chemin causal vers
la sélection de permutation et le draw. Elle ne doit pas répéter l'analyse du
cache Xenia.

## Décision

- cartographie `ShaderDef`/`StateDef`/`DrawEvent` :
  **KEEP_WITH_CLARIFICATION**, bornée aux révisions qualifiées ;
- contrat de cardinalité et de rejet : **KEEP** ;
- frontière XEX `MaterialBind` : **KEEP**, toujours ouverte ;
- pseudo-diffs anciens comme patchs applicables : **OBSOLETE** pour le
  checkout courant, mais conservés comme provenance ;
- second sink, second catalogue ou nouveau mécanisme de matching : refusés.

`PROMPTS_FOR_CHAT.md` a été mis à jour avec une demande AC6 bornée : qualifier
la frontière XEX `MaterialBind` contre le checkout courant, ou livrer
`NO_QUALIFIED_MATERIAL_BIND` avec les candidats éliminés. Cette demande ne
requiert ni VNC, ni Xenia, ni intervention humaine.

## Commandes exécutées

```text
sha256sum ac6_ordered_draw_hook_map_v1.zip
sha256sum -c SHA256SUMS
python3 tests/test_ordered_join.py
git -C .tools/ac6-recomp-reference rev-parse HEAD
git -C .tools/ac6-recomp-reference merge-base --is-ancestor 83a785d5... HEAD
git -C .tools/ac6-recomp-reference hash-object \
  src/d3d_hooks.cpp src/d3d_hooks.h src/d3d_state.h
git diff --no-index --check /dev/null PROMPTS_FOR_CHAT.md
```

Résultats : intégrité **PASS**, tests de jointure **10/10 PASS**, ancienne
révision ancêtre **PASS**, contrôle documentaire **PASS**. Aucun code natif,
projet Ghidra, XEX, cache ou sortie générée n'a été modifié ; aucune
compilation native n'était requise pour cette qualification documentaire.

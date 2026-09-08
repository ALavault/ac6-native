# AC6 retail NTSC-U/J — r468 — analyse statique (aucun run en direct) : `world=1` est détecté par UN SEUL hash de nuanceur pixel du compositeur monde ; ce nuanceur n'est JAMAIS lié dans les 573 s déjà capturées par r465 — écarte l'hypothèse du trou de montage d'assets (r452) et affaiblit l'hypothèse d'un détecteur trop étroit

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé (analyse statique du code source + relecture du
journal déjà capturé par r465 ; aucun processus lancé ce cycle).

## Contexte

Nommé par r467 (les deux rapports du cycle) : trouver ce qui gouverne
la transition `[ac6-visual-phase] world=1` dans le binaire oracle
`ac6recomp` (profil `rexglue-oracle`), en analyse statique plutôt que
par une quatrième tentative gdb en direct — les trois précédentes
(r467) ont toutes échoué à établir la cause racine du blocage
monde/campagne pour un coût en temps réel très élevé.

## Établi — l'hypothèse la moins chère écartée en premier

Vérifié `recompilation/ace-combat-6-retail/tools/prepare.py` (lignes
400-402) : le même chemin de matérialisation qui ne copiait QUE le
XEX dans `assets/` (le trou trouvé et corrigé par r452 pour le profil
`native`) existe bien ici aussi — MAIS il ne s'applique pas au
lancement oracle observé : les commandes `run_gate.py` capturées dans
les rapports r463-r467 passent l'ISO qualifiée DIRECTEMENT en premier
argument positionnel (`./ac6recomp ".../Ace Combat 6 - ....iso" ...`),
pas via `assets/`. Le contenu réel est donc déjà monté correctement ;
cette piste, la moins chère à vérifier, est écartée sans ambiguïté.

## Établi — le mécanisme exact de détection `world=1`

`recompilation/ace-combat-6-retail/upstream/AC6_recomp/src/
render_hooks.cpp:136-141` : `IsWorldRenderActive()` lit un
timestamp atomique `g_last_world_draw_ms`, rafraîchi par
`NotifyWorldCompositorDraw()` (ligne 389, un simple `store`, sans
log). L'appelant unique de cette fonction pour le backend Vulkan est
`thirdparty/rexglue-sdk/src/graphics/vulkan/command_processor.cpp:4204-4206` :
```c++
if (pixel_shader &&
    pixel_shader->ucode_data_hash() == UINT64_C(0x17e5e4ac3e713245)) {
  ac6::NotifyWorldCompositorDraw();
}
```
Le commentaire au-dessus de `g_last_world_draw_ms` (ligne 118-122) le
confirme explicitement : « stamped by the GPU command processor via
NotifyWorldCompositorDraw... a game build/render path whose
compositor shader hashes differently would never stamp it » — un
détecteur délibérément étroit, un SEUL hash de microcode de nuanceur
pixel, choisi comme signature du nuanceur compositeur du monde 3D.

**Corrélation forte, pas une coïncidence** : ce hash exact,
`17e5e4ac3e713245`, est le MÊME que celui déjà cité par r254
(2026-09-04, l'oracle de dump de nuanceurs qui a produit les 271
variantes épinglées du registre natif) dans la configuration
`ac6_neutralize_deswizzle_hashes='7d22894002d16018,
17e5e4ac3e713245:4'` — ce nuanceur compositeur est un nuanceur
DÉJÀ CONNU et déjà spécifiquement traité par l'équipe oracle de ce
projet, pas une découverte accidentelle.

## Établi — ce nuanceur n'est jamais lié dans 573 s de capture réelle

Relecture (pas de nouveau lancement) du journal déjà produit par r465
(`/fastdata/lavaulta/tmp/ac6-oracle-r465-shader-capture/ac6recomp.log`,
178 888 lignes, `--ac6_backend_log_signatures=true` déjà actif) :
**39 nuanceurs pixel distincts** apparaissent dans les champs `ps=` des
lignes `AC6 backend signature` sur toute la durée du run — **aucun
n'est `17E5E4AC3E713245`** (`grep -oiE "ps=17e5e4ac3e713245" | wc -l`
= 0). Le fix payload `neutralize` qui référence ce hash EST actif dans
ce run (confirmé ligne 7 du journal : « AC6 fix payloads:
neutralize='7d22894002d16018, 17e5e4ac3e713245:4' »). Le nuanceur
n'est donc simplement jamais soumis au pipeline graphique pendant ces
573 secondes — cohérent avec (pas contradictoire avec) tout ce que
r463-r467 avaient déjà établi : aucune route tentée n'atteint le vol
réel.

## Ce que ceci écarte et ce que ceci ne tranche pas

**Écarté (probable mais pas prouvé à 100 %)** : l'hypothèse « le
détecteur `world=1` est bogué/trop étroit alors que le monde 3D rend
réellement sous un autre hash » — un seul nuanceur, choisi
délibérément par l'équipe oracle, déjà documenté ailleurs dans ce
projet comme LE nuanceur compositeur ; rien dans les 39 nuanceurs
observés ne ressemble à un compositeur de substitution plausible
(aucune analyse de contenu des 39 hashes n'a été faite ce cycle, donc
ceci reste une inférence, pas une preuve directe).

**Toujours NON établi** : la cause racine du blocage lui-même — pourquoi
la transition campagne/monde ne se produit jamais dans les routes
tentées. Ce n'est PAS une régression de cette campagne ni du binaire
oracle : plusieurs rapports antérieurs à r454 (notamment
`reports/retail-us-mission01-flight-long-candidate-20260828.md`,
la route D5B4 de r254 elle-même limitée à « step-88 », et
implicitement chaque route tentée depuis) documentent déjà le MÊME
plafond — cette frontière préexiste largement à la campagne de
couverture de nuanceurs r454-r468.

**Observation secondaire, non exploitée ce cycle** : une seule ligne
`DeliverAPCs: normal_routine FEFEFEFE not found` apparaît à 23:56:30
(50 s après le début du run, bien avant la cinématique de 23:57:35
identifiée par r466) — `0xFEFEFEFE` est le même motif sentinelle que
r449/r450 avaient déjà rencontré (produit `native`, motif différent,
non lié). Une seule occurrence, tôt dans le boot : probablement du
bruit de démarrage, pas la cause du blocage tardif, mais non
formellement écarté.

## Décisions prises

- Analyse purement statique + relecture de journal déjà capturé ce
  cycle, conformément au périmètre borné demandé — aucun processus
  `ac6recomp`/`gdb`/`Xvfb` lancé, aucune vérification en direct
  déclenchée (la piste la moins chère, le trou de montage d'assets,
  s'est révélée déjà écartée par les journaux existants ; la piste
  suivante, le mécanisme de détection, s'est révélée résoluble par
  simple lecture de code + `grep` sur un journal déjà sur disque).
- Ne pas relancer de session gdb ce cycle — la piste statique a porté
  ses fruits sans en avoir besoin, cohérent avec la discipline de ne
  pas répéter le coût des trois tentatives déjà payées par r467.

## Gate

Aucune source de production modifiée ce cycle ; profil `native` non
touché (aucune reconstruction, aucune vérification de restauration
nécessaire).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées — aucune source de production committée ce cycle)
`ctest` racine inchangé (aucune source de production touchée ce
cycle).

## Named for r469

Le plafond campagne/monde reste la seule voie vers une couverture de
nuanceurs plus large, et il préexiste à cette campagne entière (voir
`reports/retail-us-mission01-flight-long-candidate-20260828.md` et la
limite « step-88 » de r254). Deux pistes distinctes, ni l'une ni
l'autre triviale : (1) identifier PRÉCISÉMENT où et pourquoi la
transition campagne→monde ne se déclenche jamais — nécessiterait soit
une analyse Ghidra statique du code de transition de campagne
lui-même (pas du côté rendu, cette fois), soit une session gdb ciblée
sur CE code plutôt que sur le mauvais fil (le « movie worker »,
désormais écarté par r467) ; (2) reconnaître que cette frontière est
substantielle et potentiellement hors de portée d'un cycle
d'investigation — dans ce cas, le committage du câblage
`PinnedShaderRuntime` (r433/r434/r438/r454-r467, un arriéré natif
DIFFÉRENT et sans rapport avec le sous-module `AC6_recomp`) reste une
piste non bloquante à reconsidérer indépendamment de la campagne
oracle.

## Files

Aucun artefact gitignoré nouveau (journal relu sous
`/fastdata/lavaulta/tmp/ac6-oracle-r465-shader-capture/`, produit par
r465, non modifié, non recopié).

# AC6 retail NTSC-U/J — r400 — arbre stabilisé, gate JF restauré

**Qualification** : NTSC-U/J retail, XEX SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`,
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` (PAL).
Aucun oracle utilisé.

## Contexte

Ce cycle fait suite à une demande de planification utilisateur ("passer les
blocages actuels et éviter des HUDs inventés de toute pièce"). Avant de
toucher au blocage runtime r399, l'état de l'arbre non committé (nommé
"décision de l'utilisateur, inchangée" depuis r239, ~160 cycles) a été
audité et en partie résolu, parce que le gate `mission01-final-gate-v3.json`
échouait en pratique (`evidence size mismatch`) — pas seulement en
principe.

## Établi

1. **57 fichiers racine vestiges** (contrat PAL traversal, rapports
   NDXR/NSXR/NTXR, ENTRY9, MOVE_EFFECT, NFIC, FUNCTION_*, ancien
   `README.md`, `re-agent.yaml`, un `.metadata.json` yt-dlp) confirmés non
   référencés par le fil NTSC-US actuel (`NEXT.md`/`RESUME.md`/`EVIDENCE.md`/
   `AGENTS.md`) et sans équivalent ailleurs dans l'arbre. Committés en
   suppression (`db2434a6`), après confirmation explicite de l'utilisateur.
2. **Erreur immédiate corrigée dans le même cycle** : `GLOBAL_OFFLINE_LADDER.md`
   et `XENIA_WINE_ORACLE_HANDOFF.md` étaient dans ce même lot mais sont
   vivants — le premier est cité et hashé par
   `tools/audit_ac6_global_ladder.py` (`ctest -R ac6-global-ladder-contract`
   échouait `No such file or directory` après le commit), le second est
   lecture obligatoire selon `AGENTS.md`. Restaurés et committés (`934aefc4`).
   `MISSION01_LADDER.md`, dans le même lot, reste supprimé : le texte même
   de `GLOBAL_OFFLINE_LADDER.md` le déclare déjà "ne définit plus le
   périmètre produit ni l'ordre des jalons", et aucun outil ne l'ouvre comme
   chemin.
3. **`recompilation/ace-combat-6-demo`** : 183 des 230 fichiers trackés
   (tout le `src/`, `patches/`, `shaders/`, `CMakeLists.txt`, `tests/`)
   supprimés, gardant `config/`, `docs/`, `tools/` (scripts Ghidra
   d'analyse statique) et les documents racine — conforme au texte déjà
   à jour d'`AGENTS.md` qui qualifie cet arbre d'"archive d'analyse
   statique historique", le produit actif étant
   `recompilation/ace-combat-6-retail`. Committé (`0788fb3c`).
4. **Le vrai bloqueur du gate JF** : trois fichiers avaient une preuve
   d'évidence obsolète dans les contrats (`retail_session.cpp`,
   `ntxr_texture.h`, `retail_flight_orientation.h`) — du travail en cours
   réel et cohérent (sélection de caméra par mode de vue + ancre
   free-flight NTSC-U/J dans `RetailSession::open`, lecture NTXR par clé
   GIDX, extraction pitch/yaw/roll depuis la base tournée), pas un
   correctif inventé. Reconstruit
   (`reconstruction/ace-combat-6`, `cmake --build`, exit 0), `ctest`
   86/88 (2 skips préexistants sans rapport : contenu retail absent du
   bac à sable), puis `tools/refresh_contract_evidence.py` (deux passes,
   `uncited=0`) et commit (`f2e92c64`).

**Les trois gates requis par `CLAUDE.md` sont maintenant verts** :
```
audit_ac6_mission01_native_gate.py mission01-final-gate-v3.json --require JF
  -> mission01_final_gate=audit-valid JF=pass open=none
audit_ac6_contract_artifacts.py analysis/contracts/*.json
  -> contract_artifacts=pass contracts=6 cited=189 match_head=189
audit_ac6_contract_addresses.py analysis/contracts/*.json
  -> contract_addresses=pass contracts=6 cited=321 supported=321 unsupported=0
```
C'est la première fois que ces trois audits passent tous depuis le début
de la situation r239.

## Non établi

- Le blocage runtime r399 (double-octroi de bloc par l'allocateur générique,
  `sub_8236E868` -> `sub_821F9E10`, adresse `0x10082ab0`) n'est PAS touché
  par ce cycle. Reste l'étape suivante nommée dans le plan approuvé :
  discriminant microexec (rejouer l'état pré-appel capturé en direct dans
  `MicroExecuteFunction.java` sur `sub_821F9E10` et comparer `r3`) pour
  trancher codegen vs stub hôte en un cycle plutôt qu'un dix-septième
  correctif ad hoc.
- Il reste 72 fichiers modifiés et 206 non trackés dans l'arbre
  (principalement le nouveau contenu sous `recompilation/ace-combat-6-retail/`
  et le code de surcouche HUD non tracké — voir ci-dessous) — non touchés
  ce cycle, hors du chemin qui bloquait le gate.
- **Règle HUD (§2 de la demande utilisateur) pas encore écrite.**
  `reconstruction/ace-combat-6/include/ac6/native_hud_gpu_overlay.h`/`.cpp`
  restent non trackés, dessinent des rectangles néon à coordonnées
  pixel codées en dur sans citation de preuve retail dans le code
  source lui-même (seulement dans
  `reports/ac6-native-visual-shader-hud-20260829.md`). Étape 4 du plan,
  pas encore exécutée.

## Décisions prises

- Traiter la question "committer ou restaurer" comme une ambiguïté
  matérielle nécessitant confirmation utilisateur plutôt que de trancher
  seul — fait via `AskUserQuestion` avant tout commit destructeur ;
  l'utilisateur a choisi "committer la suppression" pour les 57 fichiers
  racine vestiges.
- Corriger sa propre erreur dans le cycle qui l'a introduite plutôt que de
  la laisser pour un cycle suivant, conformément à la discipline de preuve
  du projet (§ Évidence discipline).

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
  -> exit 0
ctest (reconstruction/ace-combat-6/build) -> 86/88, 2 skips préexistants sans rapport, 0 échec nouveau
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json -> exit 0
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json -> exit 0
```

`git status` après `ctest` : encore 72 modifiés / 206 non trackés, en
dehors du périmètre de ce cycle — voir "Non établi".

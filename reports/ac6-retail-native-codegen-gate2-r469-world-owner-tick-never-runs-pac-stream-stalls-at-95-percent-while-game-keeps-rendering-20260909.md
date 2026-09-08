# AC6 retail NTSC-U/J — r469 — analyse statique (aucun processus lancé) : le tick du gestionnaire monde n'est JAMAIS appelé de tout le run, et le flux `DATA00.PAC` s'arrête à 95 % pendant que le jeu continue à rendre activement — la piste « thread figé » s'affaiblit encore, une piste « écran de confirmation manquant » se renforce

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé (relecture du journal déjà capturé par r465,
aucun processus lancé ce cycle).

## Contexte

Nommé par r467/r468 : identifier la cause racine du blocage
campagne→monde en analyse statique. r468 a tracé le mécanisme exact
de détection `world=1` (un seul hash de nuanceur compositeur,
`0x17e5e4ac3e713245`, jamais lié) et écarté l'hypothèse du trou de
montage d'assets. La sonde diagnostique déjà écrite
`ac6_world_submission_owner_probe.cpp` (committée au sous-module par
r463/r464, jamais exploitée jusqu'ici) est activée par
`--mission-render-summary` — déjà présente dans CHAQUE invocation
`run_gate.py` de r465 à r468.

## Établi — la sonde diagnostique confirme : le tick du gestionnaire monde n'est jamais invoqué

Relecture du journal déjà capturé par r465
(`/fastdata/lavaulta/tmp/ac6-oracle-r465-shader-capture/ac6recomp.log`,
178 888 lignes, 573 s) : les marqueurs `[ac6-us-campaign-service]`
n'apparaissent que **2 fois**, tous deux `sequence=1` (une seule
invocation du chargeur `rex_sub_821D5F48`, entrée à 23:55:41.205 puis
sortie à 23:55:45.173, `state_cecc=0x00000000` AVANT ET APRÈS). Les
marqueurs `[ac6-us-world-owner]` (gestionnaire monde,
`rex_sub_8226CEA0`) et `[ac6-us-mode-owner]` (`rex_sub_8219A510`)
**n'apparaissent JAMAIS, zéro occurrence sur toute la durée du run**.
Le tick du gestionnaire monde n'est donc pas seulement lent ou en
attente — il n'est **jamais appelé, pas une seule fois**, sur 573
secondes.

## Établi — le flux PAC s'arrête à 95 %, pas à la fin du fichier

Juste après la sortie du chargeur (23:55:45.173), une séquence
`NtReadFile` async vers `DATA00.PAC` démarre immédiatement
(23:55:44.505 en réalité, un peu avant le log de sortie du chargeur —
chargement concurrent sur un autre thread) : **865 requêtes** au
total, la dernière à **23:58:51.478**, offset `2 158 526 464` +
`0x3e800` = **2 158 782 464 octets lus**. `DATA00.PAC` fait
**2 267 086 848 octets** (vérifié `ls -la
game-files/DATA00.PAC`) — soit **95,2 % du fichier lu**, PAS la fin.
Les lectures s'arrêtent nettement, sans reprendre, alors qu'il reste
**≈108 Mo** non lus. Durée active de streaming : ≈3 min 07 s
(23:55:44 → 23:58:51).

## Établi — le jeu continue à rendre activement après l'arrêt du flux, ce n'est PAS un gel

Immédiatement après la dernière lecture PAC (23:58:51.478), le journal
montre des lignes `AC6 backend signature frame=15622`,
`frame=15626`, `frame=15628`… avec de vrais compteurs de tirages
(`draws=147`, `149`, `128`) et des présentations GPU continues
(`XELOG_GPU PRESENT`) — le compteur de frame est déjà dans les
15 000+ à peine 3 min 11 s après le boot (~82 fps moyen). **Ce n'est
pas un gel** : le jeu rend activement quelque chose en continu (très
probablement un écran de chargement ou une interface de confirmation
avec sa propre boucle de rendu), simplement jamais le monde 3D de vol
(le nuanceur compositeur `0x17e5e4ac3e713245` de r468 reste non lié).

## Ce que ceci affaiblit et ce que ceci renforce

**Affaiblit encore** l'hypothèse « movie worker figé » (déjà
affaiblie par r467) : la boucle busy-spin de `KeWaitForMultipleObjects`
continue de tourner UNIFORMÉMENT à travers tout cet échantillon
(visible dans les extraits autour de 23:55:45 et 23:58:51 également),
cohérent avec un sondage par tick normal plutôt qu'un blocage.

**Renforce** une piste nouvelle, non explorée par r463-r468 : le jeu
semble s'arrêter à un ÉCRAN DE CONFIRMATION (hangar, carte tactique,
ou sortie) qui continue de rendre mais n'avance jamais, parce que la
route automatisée (`routes/mission01-qualified-96.steps`) ne lui
envoie pas — ou envoie au mauvais moment — l'entrée de confirmation
nécessaire. `run_gate.py` porte déjà des drapeaux nommés exactement
pour ce genre d'étape (`--mission-hangar-confirm`,
`--mission-map-confirm`, `--mission-sortie-long-press`,
`--mission-launch-hold`, `--mission-deploy-long-press`,
`--mission-cinematic-handoff`) — non essayés en combinaison avec
`--mission-render-summary` par aucun cycle de cette chaîne
(r463-r468) à ma connaissance.

## Non établi

- **Pourquoi le flux PAC s'arrête précisément à 95 %** plutôt que
  100 % ou beaucoup plus tôt — non investigué plus loin ce cycle (peut
  être un artefact de mise en cache asynchrone banal, pas
  nécessairement lié au blocage lui-même).
- **Confirmation directe** que l'écran actif est bien un écran de
  confirmation hangar/carte/sortie plutôt qu'autre chose — inférence
  à partir des compteurs de tirages et de la nomenclature des
  drapeaux `run_gate.py` existants, pas une preuve directe (pas de
  capture d'écran relue ce cycle).
- **Aucun nouveau run en direct n'a été lancé ce cycle** — conforme au
  périmètre borné demandé (analyse statique + relecture de journal
  déjà sur disque uniquement).

## Décisions prises

- Analyse purement par relecture de journal déjà capturé (aucun
  nouveau processus `ac6recomp`/`gdb`/`Xvfb`), conforme au périmètre
  borné demandé — la sonde diagnostique déjà écrite et déjà active
  dans les runs précédents a suffi à établir un fait négatif clair
  (zéro appel du tick), sans nécessiter de nouvelle instrumentation
  ni de session gdb.
- Ne pas lancer de run en direct avec les drapeaux
  `--mission-hangar-confirm`/`--mission-map-confirm` ce cycle malgré
  l'autorisation d'un run borné unique — la piste est suffisamment
  prometteuse et peu coûteuse à vérifier qu'elle mérite son propre
  cycle dédié avec un budget dédié, plutôt que d'être tentée en fin de
  cycle sur ce qui reste de celui-ci.

## Gate

Aucune source de production modifiée ce cycle ; profil `native` non
touché (aucune reconstruction, aucune vérification de restauration
nécessaire — confirmé par `git status` sur `build/` avant de committer).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées — aucune source de production committée ce cycle)
`ctest` racine inchangé (aucune source de production touchée ce
cycle).

## Named for r470

Piste prometteuse et bon marché à tester : relancer
`run_gate.py --route routes/mission01-qualified-96.steps
--mission-render-summary --mission-d5b4-final-white
--mission-hangar-confirm --mission-map-confirm` (et/ou
`--mission-sortie-long-press`/`--mission-launch-hold`) pour voir si
fournir les confirmations d'écran manquantes fait enfin progresser la
transition campagne→monde. Si cela échoue aussi, la piste suivante
serait de relire une capture d'écran du run existant (déjà produite
par r465-r468, non relue visuellement ce cycle) pour identifier
directement quel écran est affiché à l'arrêt. Reste ouvert, non
bloquant : le committage groupé de l'arriéré `PinnedShaderRuntime`
(r433/r434/r438/r454-r468) reste une piste indépendante à
reconsidérer si le plafond campagne/monde s'avère trop coûteux à
lever.

## Files

Aucun artefact gitignoré nouveau (journal relu sous
`/fastdata/lavaulta/tmp/ac6-oracle-r465-shader-capture/`, produit par
r465, non modifié, non recopié).

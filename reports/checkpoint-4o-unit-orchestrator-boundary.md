# Checkpoint 4o — population 230 et frontière de l’orchestrateur d’unités

Date : 2026-08-12.

## Réponse courte

L’intuition est correcte au niveau de l’architecture, mais le nombre `230`
ne qualifie pas à lui seul des unités *actives*. Il qualifie la population
construite au chargement de Mission 01. La preuve courante montre un
`CX360UnitManager` retail qui reçoit les 230 entrées, puis un gestionnaire de
mission qui doit en contrôler l’activation et la désactivation. Le natif a
déjà la forme de cet orchestrateur, mais la session retail ne lui fournit pas
encore la table de vagues/événements qualifiée.

## Preuves retail bornées

- XEX PAL `default.xex`, projet Ghidra canonique
  `ghidra-projects/ace-combat-6`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Le nœud racine Mission 01 a 230 enfants dans le slot `Obj & Unit`.
  `0x820A7070` construit un objet par entrée, l’insère dans la table du
  `CX360UnitManager` (`0x82055190`) par `0x8226FEC0`, et incrémente son compte.
  Cette chaîne est documentée dans `cycle-1096` et `cycle-1127`.
- Le census bridge qualifié observe `object_count=230` constant et une
  histogramme `0x820568D4:1`, `0x82009AB0:1`, `0x82009440:228`; il n’observe
  pas encore de publication de vague, de faction ou de transition d’objectif
  (`cycle-1035`, `cycle-1080`). Cela établit la construction, pas une
  sémantique d’activité par objet.

## Ce que fait actuellement le natif

`build_retail_world` enregistre chaque entrée, l’active immédiatement et crée
un `CombatUnitState{active=true}`. La session transmet ensuite les 230 états à
`MissionExecution::launch`; `RetailWorld::published` vaut donc 230 et le
premier `WorldFrame::active_units` vaut aussi 230. Les 95 unités dont une
position de chargement est résolue et les 135 sans position restent une
partition de placement, pas une partition d’activité.

Le squelette d’orchestration générique est déjà présent dans
`MissionExecution::tick`, dans cet ordre :

```text
CombatWorld::tick
  → MissionRuntime::tick
  → MissionWaveDirector::spawn_due
  → MissionAiDirector::dispatch_due
  → MissionSequenceDirector::dispatch_due
  → synchronisation CombatUnitState → UnitRegistry
```

`MissionWaveDirector` effectue un spawn différé ou un despawn atomique dans les
deux registres. La synchronisation recalcule ensuite `active_units` après une
mort ou une désactivation. Ce mécanisme est couvert par les tests génériques
des cycles 962 et 1001.

La différence importante est dans `RetailSession::open_parsed` :
`MissionExecution` est construit avec la base d’objectifs seulement. Aucun
`MissionWaveDirector`, `MissionAiDirector` ou `MissionSequenceDirector` n’est
alimenté par le scénario retail. Le séquenceur retail (`SubMissionSequencer`)
fait avancer les étapes et les compteurs, mais il n’est pas encore relié à une
publication d’unité. Ainsi, le `230 → 230` actuel est un résultat attendu du
port partiel, pas une preuve que les 230 acteurs sont simultanément actifs dans
le jeu stock.

## Décision de portage

Ne pas remplacer arbitrairement `230` par un autre compteur et ne pas
interpréter les 135 positions absentes comme des despawns. La prochaine
divergence à fermer est un événement qualifié qui relie un `Set → Act → Order`
ou une étape du `MissionManager` à l’insertion/activation d’une entrée du
`CX360UnitManager`. Il faudra alors capturer avant/après : identifiant stable,
tick, état actif, compteur et éventuel retrait.

Jusqu’à cette preuve, `published=230` et `active_units=230` restent deux
mesures explicitement séparées dans les rapports et le comparateur de traces ;
le champ natif `active_units` absent de l’oracle est classé
`producer_schema_mismatch`, pas `value_divergence`.

## Validation

- `cmake --build reconstruction/ace-combat-6/build -j2`
- CTest complet : 81/81, avec les trois skips environnementaux connus.
- Tests Python : 141 réussis, 22 sous-tests réussis.
- Comparateur de trace Mission 01 : première différence inchangée à
  `sequence=1`, `tick=1`, `simulation_snapshot`, `active_units` absent de
  l’oracle et `230` côté natif.

Ce checkpoint ferme la clarification de responsabilité, pas le gate
`retail_units_and_waves`. Le prochain artefact recevable doit être une capture
dynamique de publication/retrait, ou une preuve statique canonique qui ferme
son owner/consumer exact.

# Cycle 1598 — retrait de la progression M01 synthétique

## Résultat

La promotion du scheduler au cycle 1580 est retirée du chemin produit. Les
trois tests alors appelés « gardes » vérifiaient seulement la présence du
contexte, du curseur et du joueur ; ils n’étaient ni les producteurs
d’objectif/combat ni les gardes retail qui autorisent le signal `-2`. Leur
succès faisait donc avancer un pas par tick et terminait M01 en six ticks sans
cible, destruction, compteur ou événement qualifié.

`play` et `replay` ouvrent de nouveau la session cache-backed en
`ExternalProbe`. `QualifiedRuntime` reste réservé mais est rejeté par les deux
overloads de session. `DiagnosticFixedTick` reste accepté uniquement par le
payload de test et refusé par le store scellé. Le rapport replay écrit
`script_drive=external_probe`, `script_advance_each_tick=false` et
`forced_progression=false`.

Le frontend, le runtime de vol, le combat diagnostic, la sauvegarde et le
replay continuent de fonctionner sans déplacer le curseur. Aucun succès ni
debrief n’est produit tant que la chaîne cible → arme/durabilité → destruction
→ compteur → gardes scheduler n’est pas reliée aux données PAL.

## Contrôles

* build : `ac6-retail-session-tests`,
  `ac6-retail-replay-trace-cadence-tests` et `ac6-native` passent ;
* CTest ciblé cache PAL : session, save/replay et cadence trace, 3/3 ;
* la garde store-backed refuse `QualifiedRuntime` et la fenêtre de 3 600 ticks
  existante reste sans progression synthétique ;
* `play --frames 1` atteint la barrière publique et écrit
  `mission01_unqualified detail=checkpoint2_scene_tcam` avant tick/PRESENT ;
* audit de complexité : `complexity_audit=pass files=284` ;
* `git diff --check` : passé.

Cette correction restaure le baseline honnête. Elle ne ferme pas la lane
objectifs/campagne, M01-C ni une gate JV/JP/JG.

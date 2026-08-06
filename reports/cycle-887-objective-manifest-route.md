# Cycle 887 — objectifs externes

Ajout de `MissionObjectiveDatabase` et de la clé optionnelle `objectives` au
manifeste. Le format TSV est `mission_id\tobjective_id\tstable_id\trequired`;
le chargement est atomique. `MissionExecution` injecte les objectifs de la
mission au lancement et expose activation/réussite pour les systèmes de
script, sans branche par mission.

Validation : CTest `2/2`, smoke SDL3/Vulkan double-frame sous Xvfb : OK.

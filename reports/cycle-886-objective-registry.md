# Cycle 886 — registre d’objectifs générique

`ObjectiveRegistry` porte des IDs stables et les états Pending/Active/Complete/
Failed. `MissionScenario` expose activation, réussite et échec explicites ;
la transition `Complete` refuse désormais une mission dont un objectif requis
n’est pas terminé. Les missions sans objectif gardent la transition existante.

Validation : CTest `2/2`, smoke SDL3/Vulkan double-frame sous Xvfb : OK.

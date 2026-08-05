# Cycle 951 — manifeste campagne natif générique

Le runtime natif expose maintenant `CampaignProgression` et un chargeur TSV
optionnel via la clé `campaign` du manifeste. Chaque ligne porte explicitement
le sélecteur, l’identifiant DPL, l’entrée physique DATA.TBL, le nombre
d’objectifs et les prérequis. Le chargeur construit un état temporaire puis ne
publie la campagne qu’après validation des doublons, références et cycles.

Le contrat couvre aussi le loadout (avion, arme, capacité qualifiée), les
états briefing/active/completed/failed, le déverrouillage par prérequis et un
snapshot déterministe `AC6CAMP`. Les snapshots vides et les ressources
physiques partagées sont acceptés; les sélecteurs restent uniques.

Validation : build CMake, CTest (`4/4`) sous Xvfb avec
`SDL_AUDIODRIVER=dummy`, et smoke SDL3/Vulkan (`swapchain_images=3`). Le test
du chargeur utilise des routes synthétiques et ne qualifie pas les missions
retail 3–15; aucune identité non prouvée n’a été ajoutée.

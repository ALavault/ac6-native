# Cycle 952 — raccord progression / MissionExecution

`MissionExecution` accepte désormais une frontière optionnelle
`CampaignProgression`. La mission doit être en état `Active` avant le launch;
les objectifs complétés, la réussite et l’échec sont propagés au HSM de
campagne après validation des préconditions. Le chemin sans campagne reste
disponible pour les fixtures et adaptateurs de développement.

Le test runtime couvre les chemins succès et échec avec objectifs externes,
ainsi que le rejet des transitions invalides. Aucune branche par identifiant
de mission n’a été ajoutée.

Validation : build CMake, CTest (`4/4`) sous Xvfb avec
`SDL_AUDIODRIVER=dummy`, et smoke SDL3/Vulkan (`swapchain_images=3`). La
sauvegarde de session n’embarque pas encore le snapshot de campagne avec le
snapshot de vol; cette jonction reste le prochain contrat de persistance.

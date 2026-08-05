# Cycle 959 — persistance du checkpoint de session

`AC6SESS` passe en version 2. Un slot peut maintenant contenir le checkpoint
MissionExecution complet : vol, HSM, objectifs, radio et unités de combat.
Toutes les chaînes, collections, identifiants, états, floats et unités sont
bornés et validés avant publication; la lecture reste transactionnelle.

La lecture de la version 1 est conservée et produit un slot sans checkpoint.
Le test couvre round-trip v2, corruption sans mutation, fichier v1 et
progression campagne. CMake, CTest (`5/5`) sous Xvfb avec
`SDL_AUDIODRIVER=dummy`, smoke SDL3/Vulkan et audit campagne passent.

Les trajectoires de projectiles en vol ne sont pas dans le checkpoint et
restent refusées par `save_checkpoint`, conformément à l’invariant du cycle
958.

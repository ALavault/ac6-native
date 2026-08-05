# Cycle 958 — checkpoint MissionExecution

`MissionExecution::Checkpoint` regroupe le snapshot de vol, l’état HSM, le
joueur, les objectifs (état et identifiant stable), l’historique radio et les
unités de combat (position, santé, faction et activité). Les collections sont
triées et bornées; une restauration invalide ne publie aucun sous-état.
Les projectiles en vol sont une frontière explicite : le checkpoint est refusé
tant qu’il en existe, afin de ne pas prétendre restaurer une trajectoire
partielle.

Le test couvre pause/reprise, objectif actif, unités et rejet d’un état HSM
invalide avec conservation du snapshot précédent. CMake, CTest (`5/5`) sous
Xvfb avec `SDL_AUDIODRIVER=dummy`, smoke SDL3/Vulkan et audit campagne passent.

Le format disque `AC6SESS` conserve encore le vol et la progression campagne;
l’encodage du checkpoint complet reste le prochain contrat de sauvegarde.

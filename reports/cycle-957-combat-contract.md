# Cycle 957 — contrat combat générique

`CombatWorld` ferme le prochain contrat runtime partagé pour les unités :
unités actives avec faction, santé et rayon de collision, armes bornées,
verrouillage inter-faction, projectiles déterministes, portée, cooldown,
collision et dégâts. `MissionExecution` initialise les unités de combat au
lancement et expose le verrouillage/le tir sans branche Mission 1.

Le test couvre doublon d’unité et d’arme, rejet d’une cible invalide,
cooldown, déplacement du projectile, impact, dégâts et désactivation d’une
unité détruite. Les validations CMake, CTest (`5/5`) sous Xvfb avec
`SDL_AUDIODRIVER=dummy`, et smoke SDL3/Vulkan passent.

Ce checkpoint ne prétend pas qualifier les paramètres retail d’armes ou de
dégâts : ils restent à extraire du corpus et à charger via un manifeste
qualifié avant toute revendication de parité.

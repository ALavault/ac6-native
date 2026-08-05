# Cycle 960 — échec destruction et expiration

`MissionExecution` propage maintenant un `Abort` automatique lorsque l’unité
joueur est détruite ou lorsque le tick limite optionnel est atteint.
La transition met le HSM en `Aborted`, marque la campagne `Failed` si elle est
active et rend un débrief `Failure`; les transitions répétées sont rejetées.
`CombatWorld::apply_damage` fournit la frontière testable pour les dommages
externes et les impacts de projectile utilisent la même primitive.

Le test couvre destruction du joueur, expiration, propagation campagne,
débrief et rejet d’un second abort. Build, CTest (`5/5`) sous Xvfb avec
`SDL_AUDIODRIVER=dummy`, smoke SDL3/Vulkan et audit campagne passent.

Le tick limite est une configuration générique : aucune valeur retail n’est
inventée tant que le catalogue de mission ne la qualifie pas.

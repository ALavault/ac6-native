# Cycle 807 — caméra et renderer natifs

`WorldFrame` contient maintenant une caméra de suivi déterministe dérivée du
transform joueur (offset fixe et cible joueur). `VulkanRenderer::render`
refuse les frames non prêtes ou sans unité/joueur et compte uniquement les
soumissions valides.

Validation : build CMake et CTest `1/1`, avec contrôles de rendu invalide et
valide.

# Cycle 1805 — régression PM4 du binaire courant prouvée

Le binaire courant conservé par le A/B du cycle 1804 a été rejoué avec la
borne exacte de 1160 ticks du contrôle positif historique, le même store, le
même backend Vulkan et les mêmes variables renderer.

Il atteint 1052 `VdSwap` et 24 draws typés, mais conserve
`RPTR=WPTR=0`, zéro soumission, zéro dword et zéro présentation renderer.
Aucun readback n'est produit. Le contrôle historique, inspecté séparément,
montre bien un logo Namco non noir.

La perte PM4 est donc une régression du binaire reconstruit, antérieure au
publisher `CP_RB_WPTR`. START et un reset tardif des compteurs sont réfutés.

Preuve persistante :
`artifacts/goal-playable/pm4-epoch-1160-current-20260824/RESULT.md`.


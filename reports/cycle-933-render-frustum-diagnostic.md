# Cycle 933 — diagnostic frustum renderer

Le rasteriseur possède maintenant un chemin de projection borné pour les
triangles qui traversent le viewport : les sommets hors écran sont ramenés sur
le bord avant la soumission, tandis que le chemin qualifié conserve le rejet
strict. Cela évite l'abandon silencieux de triangles entièrement hors écran
dans les captures développeur.

La couverture de la capture Mission 01 actuelle n'augmente pas encore : le
blocage est donc antérieur au clipping (caméra/contrat de soumission). Aucun
seuil de parité n'est déclaré atteint. CTest normal et ASan/UBSan restent 3/3.

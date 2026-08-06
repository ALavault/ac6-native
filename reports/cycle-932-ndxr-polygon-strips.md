# Cycle 932 — frontières de strips NDXR retail

Le décodeur NDXR big-endian conserve désormais les frontières des descripteurs
de polygones en injectant un primitive-restart natif entre les polygones. Le
flux est soumis comme `TriangleStripRestart`, au lieu de concaténer les index
retail impairs en fausses listes de triangles.

Validation : CTest normal 3/3. La couverture de la capture développeur reste
faible (caméra stock non qualifiée et clipping de triangles encore à fermer) ;
cette correction ne constitue donc pas une preuve de parité Mission 01.

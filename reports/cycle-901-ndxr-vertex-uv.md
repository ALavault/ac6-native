# Cycle 901 — conservation UV NDXR

Le décodeur NDXR natif conserve maintenant `u/v` dans `DecodedVertex` pour les
layouts qualifiés (UV f32 aux offsets de layout 0x0611/0x0613/0x0711/0x0721),
avec rejet fail-closed des valeurs non finies. Les fixtures textuelles restent
compatibles avec UV nuls.

Le probe BC3 existant confirme indépendamment le parcours tiled Xenos et produit
une image 512×512 réelle depuis une tranche NTXR entry 119. Les UV ne sont pas
encore consommés par le rasterizer natif : le binding texture/MATE et le sample
BC3 sont la prochaine étape.

Validations : test NDXR réel F-16 réussi ; CTest normal 3/3 ; ASan/UBSan 3/3.

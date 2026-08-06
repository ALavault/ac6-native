# Cycle 898 — caméra qualifiée bridge et couverture monde

Le générateur accepte désormais `--camera QUALIFIED_CAMERA_TSV` et copie la
matrice hors produit dans le manifeste. Avec les constantes c218–c221 du draw
27 de la frame bridge 9931, le smoke multi-terrain produit une couverture
monde visible au centre de la frame (géométrie blanche non texturée), au lieu
de la ligne quasi vide de la caméra précédente.

Cela confirme que la projection homogène et le choix de profondeur Xenos sont
corrects pour cette capture. La matrice reste explicitement bridge-only et ne
peut pas servir de référence PAL stock. Les matériaux restent des couleurs de
contrat et les NTXR/shaders réels ne sont pas encore raccordés.

Validation : manifeste généré et validé par `ac6-native --validate-manifest` ;
les suites CTest 3/3 normales et ASan/UBSan 3/3 restent vertes.

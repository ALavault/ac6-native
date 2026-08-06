# Cycle 917 — topologie NDXR primitive-restart

Le décodeur NDXR big-endian conserve désormais les marqueurs de restart
`0xFFFF` sous forme de séparateurs internes. Le renderer natif interprète les
polygones `NDXR_BE` comme des triangle-strips avec alternance de winding;
les fixtures et buffers non-NDXR gardent le chemin triangle-list historique.
Les marqueurs ne sont jamais envoyés comme indices GPU et restent exclus des
échantillons de points.

Ce correctif retire une divergence structurelle qui regroupait auparavant des
segments de strip indépendants par paquets de trois. Il ne revendique pas
encore une image retail : la caméra/transformation stock et la référence oracle
positive demeurent nécessaires.

Validation : CTest SDL dummy/Xvfb normal 3/3 après rebuild. Le chemin réel
NDXR est chargé par le manifeste développeur externe; aucun asset n’est copié
dans le produit.

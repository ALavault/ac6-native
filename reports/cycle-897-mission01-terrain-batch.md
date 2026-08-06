# Cycle 897 — batch terrain Mission 01 et clipping caméra

Le générateur de manifeste accepte maintenant plusieurs `--terrain`. Six
NDXR bornés de l'entry 119 sont décodés et soumis (1 300, 148, 517, 168,
6 218 et 862 vertices). Les assets partagés sont dédupliqués dans
`assets.tsv`, tandis que chaque drawable conserve son buffer, sa topologie et
son hash propres.

Le renderer traite aussi un drawable entièrement hors frustum comme un no-op
valide, au lieu d'échouer la frame entière. C'est nécessaire pour les LOD et
les lots partiellement visibles.

Smoke avec caméra c218–c221 bridge : sortie couleur 1280x720 et profondeur
f32 produites, mais couverture visuelle encore quasi nulle. Cette capture
reste hors preuve retail : les transforms par drawable et les matériaux NTXR
ne sont pas encore raccordés.

Validations après ces changements : CTest normal 3/3 et ASan/UBSan 3/3.

# Cycle 923 — état clavier SDL3 maintenu

`SdlInputAdapter` conserve désormais l’état pressé des huit touches d’axes.
Un relâchement ne remet plus l’axe à zéro si la direction opposée reste
maintenue; les paires opposées sont neutralisées explicitement. Le comportement
contrôleur analogique reste inchangé.

Le test couvre W+S, relâchement d’une seule direction et retour au neutre.

Validations : CTest normal 3/3, ASan/UBSan 3/3, package audit pass.

# Cycle 931 — profil SDL externe strict

Ajout de `SdlInputProfile::load_manifest`, parser fail-closed pour les mappings
SDL qualifiés hors binaire : axes, inversions et huit scancodes clavier. Les
clés inconnues, doublons, profils incomplets et valeurs hors plage sont rejetés.

Validation : test produit incluant un profil valide et un profil invalide,
CTest normal 3/3 et CTest ASan/UBSan 3/3 avec `SDL_AUDIODRIVER=dummy` sous Xvfb.
Cela ferme le contrat de mapping externe, mais ne constitue pas une preuve de
parité retail Mission 01 ; la référence oracle positive 1800 ticks reste à
qualifier.

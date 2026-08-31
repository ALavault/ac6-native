# AC6 US full-native Linux — reprise en deux paliers

Date : **2026-08-29**.

## Décision produit

NTSC-U/J Linux AMD64 devient la cible prioritaire. Le PAL reste isolé comme
référence ; aucune preuve PAL n'est réutilisée pour qualifier l'US.

Le travail est séparé en deux paliers :

1. `recompilation/ace-combat-6-retail` reste l'oracle/recompilation Linux
   Vulkan du M01 US ; son gameplay visible et contrôlable n'est pas encore
   promu.
2. `reconstruction/ace-combat-6` porte le runtime manuscrit. Il ne reprend
   aucun C++ généré et doit supprimer progressivement toute dépendance runtime
   à ReXGlue, XenonRecomp et Xenia.

## Identité US qualifiée

Les plages XDVDFS et les identités du XEX, de `DATA.TBL`, des deux PAC et des
six packs médias sont enregistrées sans octet retail dans
`analysis/oracle/ac6-recomp-ab90b-us/content-identity.json`.

Le cache natif expose désormais les profils `pal` et `ntsc-uj`. Le profil US
utilise la table à 926 entrées et le mapping campagne partagé 9..23 / monde
119..133, mais avec ses propres identités PAC et son propre `moviepack.bin`.
Les caches US par défaut résident sous le namespace `ntsc-uj`.

## Changements vérifiés

- `RetailIdentityPolicy::ntsc_uj()` et `RetailMediaPolicy::ntsc_uj()` ajoutés.
- `RetailContentStore`, frontend, campagne, session et monde sélectionnent les
  mappings depuis la cible de la policy.
- `ac6-native import|play|replay` accepte `--target pal|ntsc-uj` et refuse les
  identités incompatibles.
- `audit_ac6_retail_content_cache.py` qualifie désormais l'identité et les
  ressources M01 selon `--target`; les anciens appels PAL restent compatibles.
- Test de profil/identité, CTest ciblé et boundary audit natif passent.

## Prochaine frontière

Le palier retail doit encore fermer une seule route M01 bornée : entrée
`FlightActive`, paquet input non nul, soumission monde/HUD non synthétique et
teardown propre. Aucun A/B, GDB, trace globale, écriture guest ou optimisation.

Après ce reçu, qualifier les structures US importées puis migrer les seams
manuscrits dans l'ordre input/vol, mission/campagne, rendu, audio/médias et
frontend/offline. La sortie finale exige les quinze missions et le shell
offline complet sans dépendance ReXGlue/Xbox.

## Validation 2026-08-29

- Build CMake/Ninja Linux du runtime manuscrit : succès.
- CTest ciblé contenu, session/replay, caméra et monde : 4 pass, 2 skips de
  fixtures retail absentes.
- Pytest identité US, audit cache target-aware, validation retail et
  préparation : **28 pass, 11 sous-tests pass**.
- `audit_ac6_product_boundary.py` : **259 sources, 1 binaire, pass**.

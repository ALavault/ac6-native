# Cycle 454 — corps du dialogue de création de sauvegarde restauré

## Périmètre qualifié

- Cible : Ace Combat 6 PAL, Xenon PPC big-endian, image base `0x82000000`.
- XEX retail SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Oracle : Xenia Canary officiel `16e1eb8`, SHA-256
  `c52d27f9a115c036257efbedd91006e74964e0c12aebb09b0c1dd93a31280f9a`,
  Wine 10.0, Vulkan/FBO.
- Runtime Linux corrigé SHA-256 :
  `ef9c629e6f0bd2c5bb5a6194e47c124ac5b23dc0673f38318e4f2953b65488e5`.
- Aucun fichier généré par le recompilateur n'a été modifié.

## Diagnostic confirmé

La séquence native minimale `Start`, puis un seul front `A` de 100 ms mène au
panneau invité avec `YES/NO`. L'état de sauvegarde est légitime : le chemin
invité sélectionne `mode388=1`, termine ses opérations asynchrones avec
`result=0`, puis entre dans l'état UI 9. L'injection d'un élément de contenu ne
change pas ce résultat ; l'énumérateur vide n'est pas la cause.

Un profil oracle temporaire a été créé par copie de l'exécutable, de la
configuration et du compte, sans le répertoire de sauvegarde AC6. Le profil
original et sa sauvegarde n'ont pas été modifiés. L'oracle vierge montre :

1. `ACE COMBAT 6 Game Data not present on storage device.` avec `OK` ;
2. après un second `A`, `No ACE COMBAT 6 Game Data.` puis
   `Create new Game Data?` avec `YES/NO`.

Captures oracle :

- `reports/logs/cycle-454-clean-profile-oracle/t48-first.png`, SHA-256
  `bdc5e56c9b568abdfb1374da45fa7798c08631b55b1cbaf0086ab3ed7e6bc58e` ;
- `reports/logs/cycle-454-clean-profile-oracle/t52-second.png`, SHA-256
  `f7716940466c3141420b150a2d229932ff96f189ffa2f174c6bbff6e08e85e96`.

Le second panneau correspond exactement à la géométrie et aux boutons du
panneau natif vide. Sa clé de corps est `M70000_122`. Dans le catalogue PAL
chargé par le natif, elle résout l'identifiant 1088 mais sa séquence de 13
glyphes ne produit aucun pixel avec l'atlas actif. La clé voisine 1089 contient
`Save Replay Data?` et constitue un contrôle négatif, pas un correctif.

Confiance : chemin de sauvegarde, clés, identifiants et captures `confirmed /
dynamic`; cause interne exacte du désaccord catalogue/atlas `unknown`.

## Correctif natif

`src/ac6_dialog_text_fallback.*` reconnaît exclusivement `M70000_122` à la
frontière du renderer invité. Il ne modifie ni la clé, ni le catalogue, ni les
boutons invités. Il publie un signal de présence de 250 ms à la couche de
présentation native.

`src/ac6_native_graphics_overlay.cpp` complète alors uniquement le corps
manquant avec les deux lignes observées dans l'oracle. Le panneau, la sélection,
les boutons et les entrées restent rendus et traités par le jeu. Le signal
expire dès que la clé n'est plus soumise ; le texte ne persiste donc pas sur
l'écran suivant.

Le mode littéral `##` du renderer invité a été testé avant ce chemin : le repli
s'activait mais restait vide dans ce contexte de police. Cette hypothèse est
invalidée et aucun détournement `##` ne reste dans le correctif.

## Validation

- Configuration CMake après ajout des sources : PASS, action
  `638ffd3b24c6b3ee77a1d47c02a73c67b4c6a8c8cd01702e67a1a28efc069ac5`.
- Build ciblé `ac6recomp ac6_dialog_text_fallback_test -j16` : PASS, action
  `5b5e156b3aba966d6ea00efe9d47a753e4883e95338011e282dc5a38021992ba`.
- Tests AC6 ciblés : 3/3 PASS, action
  `6761b5ab4ddcd09a4d2dc219ded9eeff4f982c3416560ade184c486d48ab01d8`.
- Validation visuelle minimale : PASS, action
  `76a96b0243dc97b5e4724b5e87e9180d0309d794221299fdfd4208b9bc5790f6`.
- Capture native finale :
  `reports/logs/cycle-454-native-dialog-body-fallback/t48.png`, SHA-256
  `1ac535e56b32fe736284472a93f830e5350814ed6ba50ef1a6240aeebd8b32f6`.
- `git diff --check`, `bash -n tools/ac6-run.sh` et absence de
  `build-rt/bin/bin` : PASS.

Une validation non destructive supplémentaire a confirmé le choix par défaut
`NO` à 47 s. Le journal hôte voit la touche et l'invité reçoit bien
`buttons=0x1000`, mais le dialogue reste affiché jusqu'à 56 s. Action
`5eaa6f31498ed7405364964c4ebeb2f9514157a1b73160d7134c595bc3a1d6c7`.
Cette mesure n'invalide pas le correctif de texte ; elle confirme une seconde
frontière fonctionnelle en aval.

Le build global reste bloqué hors du périmètre AC6 par le binaire SDK
`powerpc-none-elf-as` non exécutable (code 126). Les trois tests natifs AC6 ne
dépendent pas de cet assembleur et passent.

## Limites restantes

- Le corps vide du panneau `YES/NO` est corrigé et visuellement validé.
- Le natif atteint ce second panneau avec un seul `A`, tandis que l'oracle
  vierge exige un premier acquittement `OK`. Cette divergence de transition est
  distincte et reste à qualifier avant toute revendication de parité retail.
- Un nouvel appui `A` reçu par l'invité (`0x1000`) ne confirme pas `NO` dans la
  fenêtre testée. Le dialogue est donc lisible mais pas encore franchissable ;
  la prochaine frontière est le populator de confirmation qui conserve
  `flag[+25]=0` et `mask[+28]=0` dans les traces existantes.
- Le correctif est un repli natif exact et borné. La raison pour laquelle le
  couple catalogue/atlas PAL ne dessine pas l'enregistrement 1088 reste
  `manual-review`; elle ne doit pas être masquée par un remappage vers une
  chaîne adjacente.

État : défaut visuel demandé corrigé ; parité complète non revendiquée.

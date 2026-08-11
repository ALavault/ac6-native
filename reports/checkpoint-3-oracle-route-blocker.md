# Checkpoint 3 — frontière résiduelle de la route oracle

Date : 2026-08-11

## Résultat

Le problème « dialogue de vidéo audible, image figée » était déjà identifié
dans `checkpoint-2-oracle-route-requalification.md` et
`checkpoint-2-media-xma-contract.md`. Les observations courantes le précisent :
le dialogue peut continuer pendant que la présentation ou la sortie de vidéo
ne progresse plus. Sous `SDL_AUDIODRIVER=dummy`, seule la progression de route
est qualifiable ; l'audibilité ne l'est pas.

Le checkpoint 3 reste ouvert. Aucun des essais courants ne fournit encore deux
fenêtres oracle identiques de 3 600 ticks. Le raster ou le runtime natif ne doit
donc pas être ajusté à partir de ces faux départs.

## Corrections conservées

- `0x822A6710` identifie le joueur mais n'est pas une horloge continue : le run
  I ne produit que 62 échantillons. La trace est maintenant émise après le tick
  manager qualifié `0x8226D1C8`, avec le dernier joueur exécuté.
- La route de trace s'arrête au prompt de déploiement et pulse A jusqu'au vrai
  événement `0x822A6710`; elle ne confond plus un délai de capture avec le vol.
- Le runner conserve le suffixe non consommé d'un chunk de log. Le run N avait
  publié joueur et manager sur deux lignes consécutives, mais l'ancien runner
  supprimait la seconde avant le `wait` suivant.
- La pile de 11 patches s'applique proprement à `dcd41b7457f` : 27 fichiers,
  arbre `9cbce31057b6a52c010fee763b8a38e877890ce85651ce1037ad8ac6af22fa5b`,
  profil capture identique octet pour octet. Aucun C++ généré n'a été édité.

## Validation négative finale

Le binaire final de récupération a le SHA-256
`37e0c88d73b917a3b10a11bed44d5c42908032a3a1a0949a652abdec6d6432e7`.
Le run M atteint l'intro de campagne, puis reste avant la transition `0→1`
malgré les pulses bornés : 57 étapes, 478,8 secondes, aucun fatal. La capture
intro vaut `9344481fe6bb8f2172047c98d5d0fd65e399232d293ed84547e3655ab3112b96`;
le log vaut `062d4c619bac7068db093731d936bcd24ad2c11238fc39f7e61b526efa09fa3c`
et le manifeste `c9b0b1693fa132519081cb133ddd8c75a5bf43da2fcf09f05d351b1dd13edee7`.
L'interruption restaure exactement `/dev/shm`, supprime son segment détenu et
ne laisse aucun processus.

Une capture G antérieure avait déjà montré deux images identiques à plus de
30 secondes d'intervalle pendant que le dialogue restait audible en session
interactive. Les runs H/I ont franchi la vidéo après la première correction de
cadence audio, ce qui prouve que le défaut est intermittent et que ce patch
n'est pas encore une garde de reproductibilité.

Le run R, avec le même binaire `37e0c88d…` et la nouvelle barrière
`0x8237C828`, apporte une frontière différente : la route atteint bien la fin
du gestionnaire de sauvegarde (`state40=0`, `selector44=0`, `type28=10`), crée
les pipelines de l'écran suivant, puis reste sur `LOADING`. Aucun marqueur
`ac6-campaign-transition` ni `ac6-first-mission-task` n'est émis. Après
421,5 secondes et 58 étapes, la capture Loading est stable
(`162d7cbd472d5c90d41a72b2203323ebae523a0ed5a6934bf54303bd32a16f7c`), sans
fatal et sans fuite `/dev/shm`. Le worker audio est à nouveau dans une attente
conditionnelle au prélèvement GDB, donc ce run ne justifie pas une nouvelle
correction audio.

Le warning `CompleteOverlappedEx: missing XEvent for handle FEFEFEFE` observé
sur R est l'anomalie déjà séparée par les cycles 434/437 : elle concerne un
autre `overlapped` que le sélecteur et ne peut pas être retenue comme cause
sans un lien dynamique nouveau. Elle reste un indice, pas un correctif.

Le probe de scheduler confirme la coupure : R ne crée même pas
`mission01-frame.raw.jsonl`, alors que H/I publient les trois événements
`0x82267370` attendus avant leur fenêtre de trace. La première divergence
qualifiée de R est donc antérieure au scheduler de frame, après la fin du
gestionnaire de sauvegarde ; elle n'est pas une absence de soumission Vulkan
ou un défaut du raster.

Le témoin positif S empêche toutefois de transformer cette coupure en cause
générale. Avec le même binaire, le même profil frais et la même
`trace-entry`, S franchit `state0→1` à `00:00:09.175`, `state1→2` à
`00:00:17.988` et atteint le prompt de déploiement. Le run est qualifié
(`oracle_run=pass`, 76 étapes, 234,15 s, aucun fatal, `/dev/shm` identique
avant/après) ; son binaire vaut `37e0c88d…`, son log
`e02acd27266d11250bc5aeb615143670cb6f04d59e220ab4df1d8078783f5d1e`, son
manifeste `bdb3fb92e752e5d05be8f92396627a20f486c253b528e476f87add53b2d7780e`
et sa capture finale de briefing `41fe632add7bea595cbcfc63825e8ca1c7c6dcad4fb737af80036e6ff7c61057`.
La frontière est donc intermittente ; aucune nouvelle correction audio ou
Vulkan ne doit être déduite de R seul.

Le run T de la route contrôlée confirme la non-déterminisme en amont : même
SHA binaire `37e0c88d…`, mais interruption après 204,49 s et seulement cinq
étapes, avant tout `ac6-save-route`, sans fatal. Son log vaut
`d96c0ab13142fb5b5eed2f68d00b621143a332849d3f68ff44e75ef3df068955` et son
manifeste `6f0bc60787b31c13f96ee253367d0f7d3d28f8325ac18ce5397755148f546e83`.
Ce négatif n'est pas fusionné avec S : il ne ferme aucune divergence native et
ne fournit pas de fenêtre de trace.

Le variant U, qui ajoute une seconde après la barrière scheduler initiale,
réduit le faux départ et atteint `state0→1`, `state1→2`, le briefing et le
prompt de déploiement. Il retombe ensuite sur la frontière vidéo connue : les
captures `post-start-a`, `post-start-b`, `post-weapon-confirm` et
`deploy-prompt` ont toutes le SHA `9344481fe6bb8f2172047c98d5d0fd65e399232d293ed84547e3655ab3112b96`
(écran noir avec l'overlay), sans frontière joueur/manager ni trace armée.
Après 280,96 s, l'interruption est propre, sans fatal et avec un seul segment
créé puis nettoyé ; log `a7d4b54218eca49667a77f25b3ffbdf3b1a6b0cb378d693240f955d3aa5a7b2c`,
manifeste `2f0e8ef2d5841b5ea2e3b504bfa65687d2c2f9ac828c3bbb3310e4b54c60db2f`.
Sous `SDL_AUDIODRIVER=dummy`, cela qualifie le gel visuel et la frontière
d'exécution, pas l'audibilité elle-même ; le dialogue audible reste donc
l'observation interactive historique, déjà documentée.
U ne crée pas non plus `mission01-frame.raw.jsonl` : aucune soumission de
frame n'est demandée après le lancement, ce qui recoupe la mesure historique
du cycle 316 (invité qui cesse d'appeler `VdSwap`) et écarte à nouveau un défaut
de présentation hôte.

## A/B audio–présentation en niveau debug

Les journaux U précédents étaient au niveau `info` et ne rendaient pas les
compteurs XAudio. Deux routes capture-only ont donc été rejouées avec
`REX_LOG_LEVEL=debug`; leurs routes, manifestes, journaux et compteurs bornés
sont consignés dans
`analysis/oracle/ac6-recomp-dcd41b/probes/video-frontier-debug.json`.

Le négatif `video-frontier-diagnostic` expire après 306,98 s et cinq étapes,
avant toute transition de campagne ou appel `0x82267370`. Pourtant le worker
audio soumet 27 660 trames, en consomme 27 659 (`queued_played=27 659`) et
reste à profondeur 1 ; les 9 241 underruns sont des injections de silence du
périphérique dummy. La présentation n'est donc pas bloquée par une absence
de client audio.

La variante temporisée, qui reprend la seconde d'attente du témoin U, est
positive : 77 étapes en 195,24 s, transitions `0→1` à `00:34:57.611` et
`1→2` à `00:35:06.445`, 9 913 `PRESENT` et capture finale
`e2be0487deb99266c66ef941767cca0f1a2905f79b9a035ec1a565dc8d460653`. Au
prélèvement audio, 23 360 trames sont soumises, 23 357 consommées et la file
reste à trois éléments ; ce passage ne gèle donc pas l'image vidéo.

Cette paire ferme seulement une confusion : la consommation audio et la
progression de présentation sont indépendantes dans le backend dummy. Elle
ne transforme pas le dialogue audible en preuve sous SDL dummy et ne fournit
toujours pas la fenêtre de trace 3 600 ticks.

La route complète historique a ensuite été rejouée avec le binaire final
`37e0c88d…`, puis la trace armée seulement après les captures HUD. Elle
atteint bien les étapes de lancement, mais les huit captures de
`mission-launch` à `flight-throttle` sont identiques
(`d31aab0a5303258ff36ba2588e16fefa750dbb93437cbdfbf4102117dcdd549b`) et
aucun `mission01-frame.raw.jsonl` ou événement `0x82267370` n'est créé. Après
461,92 s, le wait `AC6 oracle trace v2 complete: 3600 gameplay ticks` expire
sans fatal ; manifeste `46a738fe70025bb3ff14da277cf2a74bc913fa2511ca54e08a172989c12b663d`
et journal `bbc0fc156dc89be8b74572aae9b629ea03460ed1ce9549f57b10fafcdd9a6994`.
Cette route ferme donc le même front post‑transition avec une séquence d'entrée
différente ; elle ne justifie toujours pas une divergence native.

## Frontière dynamique

Un arrêt GDB borné sur M exclut deux explications trop larges :

- le worker audio est endormi dans `KeWaitForMultipleObjects`, via
  `0x823B4350 → 0x823BE6D8`; il ne boucle pas dans Vulkan ;
- le thread invité actif passe par `0x8237A2D8`, appelé depuis `0x82375290`,
  avec `r5=16` et `r8=256`. Ces bornes sont saines et n'indiquent pas un compteur
  de filtre corrompu.

La prochaine preuve doit joindre le wait audio à son signaleur et à la
transition de fin de vidéo, puis comparer ce triplet entre un passage et un
gel. Aucun nouveau correctif de cadence, portage statique ou fallback produit
n'est justifié avant cette première divergence exécutée.

## Gardes passées

- `ac6_decimal_parse_test` et `ac6_audio_startup_pacing_test` : pass ;
- CTest produit : 73 tests passés, 1 test de ressources frontend explicitement
  sauté par son contrat d'absence d'artefact ;
- tests Python runner/trace/replay/oracle ciblés : 49 passés, 8 sous-tests ;
- application propre et correspondance du profil capture : pass ;
- `git diff --check` : pass sur le périmètre conservé.

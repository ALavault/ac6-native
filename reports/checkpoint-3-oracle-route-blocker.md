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
- tests runner/trace/replay/oracle ciblés : 46 passés, 8 sous-tests ;
- application propre et correspondance du profil capture : pass ;
- `git diff --check` : pass sur le périmètre conservé.

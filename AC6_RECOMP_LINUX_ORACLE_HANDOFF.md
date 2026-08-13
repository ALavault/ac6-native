# AC6 ReXGlue Linux/Vulkan oracle handoff

Ce handoff est le seul chemin de nouvelle capture comportementale. Le handoff
Xenia/Wine reste réservé à une frontière statique nommée et ne produit aucune
preuve de parité.

## Identités et portée

- oracle : NTSC-U/J `default.xex`, SHA-256
  `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`,
  Media ID `531C30BE`, XXH3 `892639B654015428` ;
- runtime : `sal063/AC6_recomp` commit
  `ab90b54713e5889f33eee1cc8681dae89fe83d1e`, arbre
  `1e60427e316a2667d189eb1e067a8ec7d776fd50` ;
- produit comparé : PAL `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- usage : inputs poll-exact et observations structurées uniquement. Aucun
  rendu oracle, code généré ou nom généré n'entre dans le produit.

Avant lancement, le runner doit vérifier XEX complet, Title/Media ID, version,
XXH3, commits et arbres, pile de patches, configuration stock, arbre généré et
hash du binaire. Il refuse les améliorations AC6, le fallback physique et tout
autre contrôleur. Il échantillonne `CLOCK_MONOTONIC_RAW` avant le marqueur
`0x821CA940` sans inférer lui-même la cadence.

O1 est scellé par `linux-vulkan-minimal-v1.json` et validé par
`tools/ac6_recomp_linux_oracle.py` : boot jusqu'au titre, polls continus, trois
captures Vulkan consécutives non noires et arrêt propre. Le contrôle négatif
sans contrôleur passe ; la présence d'un contrôleur physique unique reste une
condition du handoff O2 ci-dessous.

## Handoff O2 — 75 minutes maximum

Partir deux fois d'un processus et profil propres. Choisir Normal/Normal,
anglais, avion/arme `1/1`, avec un unique contrôleur XInput et aucun autre
input. Enregistrer : boot neutre ; menu et tutoriel ; sortie aérienne de 3 600
ticks ; Mission 01 complète jusqu'au debrief succès. Refaire chaque séquence
une seconde fois. Faire aussi le contrôle négatif contrôleur absent, sans
fallback physique.

La seule dérivation de cadence permise est identité 60→60 ou ZOH exact 30→60.
Sceller raw v4, census v2, fenêtres, reçu v4, `AC6RTPLY` v3 et trace v3. Ne
conserver dans Git que les inputs normalisés, observations bornées, manifests
et hashes. Le statut ne devient `captured` qu'après deux passages poll-exact et
observation-exact et qualification de cadence.

Artefacts attendus : trace v3, replay v3, reçu v4, census v2, manifests des
fenêtres, logs de lignée et preuve de trois `PRESENT` Vulkan non noirs
consécutifs. Interdire tout container retail ou fichier de 512 MB ou plus.

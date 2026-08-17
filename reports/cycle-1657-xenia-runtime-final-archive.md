# Cycle 1657 — valorisation de `xenia-runtime-results-20260815-final`

## Verdict

Oui, l’archive est valorisée comme oracle Xenia durable, mais elle ne ferme
aucun lane du gate PAL recompilé. Son SHA est
`0196ab3630a937118abea3d41e6d3dc663fcfdb04a3d7d2a843d572361578768` (8 616 470
octets). Le manifeste et les fichiers sélectionnés ont été vérifiés en lecture
seule. Le run réussi porte le title ID `4E4D87E6` et un hash externe
`141e9f25…2b117`, pas le SHA exact de `Default.xex` PAL
`de917873…5da8`; aucune identité binaire PAL n’est donc promue.

## Ce que le run fournit

`20260815-162258` (Xenia `7010c86fb14f118ee598d3f76010dc0759b9502a` + patches)
franchit l’écran noir, le titre, les menus, la cinématique pré-mission et le
gameplay. La fenêtre stable documentée est de 30,539 secondes : 911 frames,
29,83 fps, et 5 709 soumissions audio. Le flux complet contient 72 488
événements sur 328,963 secondes : 12 496 `frame_swap`, 59 969
`audio_submit`, 23 créations de threads. Les arguments de swap observés sont
`{0xBA9A0000,0x1A9A0000,1280,720,6}` au PC guest `0x821C5A1C` ; les submits
audio passent par `0x8234D360`.

Les 2 113 lignes sémantiques contiennent 657 snapshots watchdog sans attente
résiduelle, 655 snapshots PC, 790 lectures et 11 ouvertures de fichiers. Les
19 lignes contrôleur (thread 6) donnent notamment les masques `0x10`, `0x400`
et `0x1000`. Quatre PNG 1 853×1 011 sont présents (titre, cinématique,
gameplay et gameplay soutenu) ; leurs SHA sont conservés dans la capsule et
restent `oracle-only`, jamais une screencap du recompilé PAL.

## Limites mesurées

Après environ 280 secondes guest, la cadence chute : 29,9 fps à 250–260 s,
27,6 à 280–290 s, 20,1 à 290–300 s, 15,1 à 310–320 s. Le log comporte 36
récupérations ALSA underrun rapportées. L’archive ne permet pas d’isoler si la
cause est CPU, GPU, audio ou accumulation de file ; elle ne doit pas être
présentée comme une preuve de stabilité longue durée. `exit_status=137` signifie
également que la terminaison n’est pas une sortie propre qualifiée.

La comparaison est néanmoins utile : le run natif gelé
`20260815-145806` bloque les threads invités 6/17 dans
`NtSignalAndWaitForSingleObjectEx` (`0x821A69C8`, `0xF8000088→0xF800008C`),
alors que `20260815-150700` progresse sous `strace` avec forte perturbation
d’ordonnancement. Le run patché montre que la correction POSIX lève ce défaut
sur cette exécution Xenia.

## Qualification

- `xenia-generic` : événements, PCs/LRs, threads, cadence, comparaison du
  blocage POSIX et provenance Xenia.
- `demo-observed` : étiquette title ID AC6 Demo, transitions et images telles
  qu’observées par Xenia.
- `demo-qualified` : aucun élément, faute de jointure au SHA PAL démo.
- `unknown` : parité avec `ac6-demo-recomp`, identité PM4/IB/microcode, pixels
  guest-owned, résultat de mission et cause exacte du ralentissement.

Les dumps shader présents dans l’archive n’ont pas été copiés, suivis ou
utilisés comme containers PAL. Aucun fichier Xenia, Ghidra, C++ généré,
microcode ou actif propriétaire n’a été modifié.

Capsule : `analysis/demo/ac6-xenia-runtime-final-archive-v1.json`.

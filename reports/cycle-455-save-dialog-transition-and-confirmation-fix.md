# Cycle 455 — dialogue de création franchissable jusqu'à `GAME DATA`

## Périmètre qualifié

- Cible : Ace Combat 6 PAL, Xenon PPC big-endian, image base `0x82000000`.
- XEX retail SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Runtime Linux final SHA-256 :
  `38f88c1e9f894c8657bab0fc1a093452fec4bc52564c1bfc5316f869e576555c`.
- Aucun fichier de `generated/` n'a été modifié.

## Pourquoi le natif montre directement `YES/NO`

Ce panneau n'est pas une UI hôte de secours. Le jeu invité le demande lui-même.
Sur un stockage utilisateur natif vierge, la trace qualifiée donne :

- utilisateur 0 accepté par `sub_821CFE18` (`predicate=1`) ;
- sélecteur de stockage terminé avec le périphérique 1 ;
- scan de contenu terminé avec `result=0` et zéro élément ;
- machine interne `0 -> 1 -> 2 -> 5 -> 3 -> 9` ;
- publication du type de dialogue 30, le panneau `YES/NO`.

Il n'existe aucune publication antérieure d'un dialogue monobouton dans cette
exécution. Le changement de locale ne peut donc rien modifier : la divergence
est déjà dans la transition de sauvegarde, en amont du catalogue de texte.
L'oracle vierge montre bien un avertissement `OK` avant le type 30, mais sa
condition exacte n'est pas observable dans le journal Xenia conservé. La cause
native est `confirmed/dynamic`; l'écart précis de contrat XAM/timing qui fait
prendre à l'oracle la transition préliminaire reste `needs-dynamic-evidence`.
Forcer seulement le champ de type 4 ou le prédicat de connexion n'a pas republié
le modal déjà verrouillé et a donc été retiré.

## Correctifs natifs

`src/ac6_save_dialog_input_bridge.h` convertit les fronts déjà calculés par
l'invité en réponses de la machine de sauvegarde : sélection par défaut `NO`,
`Left -> YES`, `Right -> NO`, puis `A -> 1/2`. Le même pont acquitte les notices
monobouton des états de création 37 et 35. Il ne synthétise aucun appui et ne
s'exécute que dans les états d'attente qualifiés.

Le premier passage complet a ensuite exposé un défaut indépendant du code
généré : le parseur CRT retail `sub_82387530` contient une boucle interne vers
`0x823876E4`, mais le générateur l'a émise comme appel non résolu depuis
`0x82388068`. Le format `%s%08d.dat`, utilisé après création, avortait donc le
processus. L'override fort reconnaît uniquement les flux chaîne retail
(`flags=66`) et les route vers l'implémentation PPC `_vsnprintf` déjà fournie
par le runtime, en conservant curseur et compteur du flux. Les autres flux
restent sur l'implémentation invitée.

## Validation

- Build ciblé `ac6recomp ac6_save_dialog_input_bridge_test -j16` : PASS,
  action `c7ec68bce9f8f2bded5718a4d7d03cc63f57be98be1a0f43a430b9efcec5ce58`.
- Tests natifs AC6 ciblés : 2/2 PASS, action
  `596582513a3dfa33708caa4adcf409e3918c1a01ad2b224e5b1f6b9db75d27b3`.
- Exécution vierge bornée : PASS, action
  `134e1c8dd1006476ba63fc0c0571b6724ce69f65428fa6aa013b6d56f5a50d68`.
- Trace archivée :
  `reports/logs/cycle-455-complete-save-dialog-pass/ac6recomp.log`, SHA-256
  `bf3d1fbf8606f3b3c7ba0732c8b6b354155c7c49c7e047caae3f8a322504273a`.
- La trace confirme successivement `type=30 response=1`,
  `type=37 response=1`, `type=35 response=1`, puis l'état externe 8.
- `save.dat` (129112 octets), son en-tête et `not_00000000.dat` sont créés
  uniquement dans le stockage isolé du dossier de preuve.
- Capture stable `GAME DATA` à 66 s : SHA-256
  `eb87a9ad72914737d02644e68c3cc245c4afbf4df33649e65ed59d9e6473fa64`.
- Capture stable `GAME DATA` à 72 s : SHA-256
  `36ab1763de3f87442705f636c7515f3a100ce544cc0c89db809cbcedf58c9986`.
- `git diff --check`, `bash -n tools/ac6-run.sh` et absence de
  `build-rt/bin/bin` : PASS.

Le corpus PPC complet reste hors de cette validation : son assembleur SDK
vendu n'est pas exécutable (code 126), blocage déjà qualifié et non répété.

## Risques résiduels

- Le dialogue est franchissable depuis un stockage vierge et l'écran suivant
  est atteint, mais la première boîte `OK` de l'oracle n'est pas encore restaurée.
- Les notices 37/35 réutilisent brièvement le panneau visuel verrouillé avant sa
  fermeture. La parité visuelle de cette sous-séquence n'est pas revendiquée.
- Le texte `GAME DATA` est transitoirement instable dans la première image après
  fermeture (63 s), puis stable dès 66 s ; aucune corruption persistante n'a
  été observée dans la fenêtre bornée.

État : passage fonctionnel confirmé ; parité exacte du premier `OK` encore
`needs-dynamic-evidence`.

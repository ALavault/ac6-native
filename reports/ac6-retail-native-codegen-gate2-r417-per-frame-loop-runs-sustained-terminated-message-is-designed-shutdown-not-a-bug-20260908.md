# AC6 retail NTSC-U/J — r417 — corrige r416 : la boucle par image tourne bien de façon soutenue (~16ms/itération, ~292 appels sur 5s), « terminated its own thread » est le mécanisme d'arrêt PROPRE et voulu (r277), pas un nouveau blocage

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r416 (nommé pour r417) : lire le code PPC de
`sub_821D7AE0`/`sub_821D7CD0` et de leur appelant pour déterminer si
l'arrêt après deux itérations est une sortie normale ou un nouveau
blocage.

## Correction

**r416 se trompait sur la nature de l'arrêt.** La lecture du code PPC
de l'appelant, `sub_821D7DE0` (`ppc_recomp.23.cpp:22881`), montre une
boucle INCONDITIONNELLE et véritablement infinie (`goto loc_821D7E84`
sans aucune condition de sortie dans le corps de la boucle lui-même) —
appelant `sub_821D7AE0`, `sub_821D7CD0`, deux appels indirects (tables
de fonctions), puis `sub_82331E78`, à chaque itération. **Aucun bug
dans cette boucle.**

## Établi

### La boucle tourne réellement, de façon soutenue, à une cadence réaliste

`r427_wait_args.gdb` — point d'observation sur
`__imp__NtWaitForSingleObjectEx`, filtré par pile d'appel contenant
`sub_821D7DE0` (fenêtre `8000ms`) : **292 déclenchements**, dont les
derniers (`#283`-`#292`, pile `sub_821F7538<-sub_82345CE0<-
sub_8233B5A0<-sub_82331E78<-sub_821D7DE0`, c'est-à-dire l'appel direct
`sub_82331E78` du CORPS de la boucle, pas via `sub_821D5F48`) montrent
un **écart quasi constant de ~16-17ms entre déclenchements successifs**
— la cadence d'une boucle par image à ~60 Hz, pas un blocage.
`wait_mode=0x1`, `alertable=0x0`, `timeout_ptr=0x0` (attente sans
délai explicite côté appelant, mais l'objet est signalé à intervalles
réguliers par ailleurs — cohérent avec un minuteur/sémaphore de cadence
d'image).

### Le catch `GuestThreadTerminated` est un mécanisme d'arrêt PROPRE, conçu délibérément (r277), pas une preuve de blocage

`r426_throw_site.gdb` (`catch throw`, capture de la pile exacte au
moment du lancer C++) :

```
#0 __cxa_throw
#1 __imp__NtWaitForSingleObjectEx
#2 sub_821F7538 <- sub_82345CE0 <- sub_8233B5A0 <- sub_82331E78
   <- sub_821D7DE0 <- __xstart <- ...::_M_run()
```

`recompilation/ace-combat-6-retail/tools/materialize_native_import_
stubs.py` (générateur des stubs natifs, non gitignoré) confirme :
les stubs d'attente générés vérifient `native_guest_threads_
stop_requested()` (commentaire explicite « r213/r277's stop_
requested() check ») et lancent `ac6::native::GuestThreadTerminated{}`
quand ce drapeau est mis — exactement le mécanisme documenté par
`artifacts/retail-us-native-r277-guest-thread-teardown/README.md` :
r277 a délibérément construit ce mécanisme pour éviter un SIGSEGV de
fin de processus (les threads invités détachés survivaient au
`shutdown()` et accédaient à une mémoire déjà libérée). **Le message
"generated entry terminated its own thread" apparaît donc, dans
`ac6recomp_main.cpp:175`, précisément lorsque la fenêtre de sonde
s'écoule et que le harnais demande l'arrêt propre — ce n'est PAS un
symptôme de bug côté jeu, c'est le comportement CONÇU et attendu de
l'outillage de test lui-même.**

## Ce que ceci établit

**r416 avait raison sur les faits observés (`sub_821D5F48` retourne,
`sub_821D7AE0`/`sub_821D7CD0` sont atteints, `presented_frames` reste
`0`), mais tort sur leur interprétation** ("nouveau point d'arrêt,
pas résolu"). Corrigé : la boucle par image tourne réellement et de
façon soutenue tant que le harnais de sonde la laisse tourner ; son
arrêt en fin de fenêtre est le comportement voulu du harnais, pas un
défaut. **La vraie question ouverte se réduit à : pourquoi
`presented_frames` reste `0` malgré une boucle par image qui tourne
visiblement à un rythme réaliste ?** — une question probablement sans
rapport avec toute la chaîne allocateur r399-r415 (l'allocateur ne
bloque plus rien, r414/r415), et plus probablement liée à l'absence
d'une vraie surface d'affichage/swapchain dans ce harnais de sonde en
ligne de commande sans fenêtre, ou à un chemin de présentation gaté
par autre chose.

## Non établi

- **Pourquoi `presented_frames` reste `0`** malgré la boucle qui
  tourne — piste non explorée ce cycle : le chemin de présentation
  (`sub_821D7AE0`/`sub_821D7CD0` eux-mêmes, ou plus en aval) n'a pas
  été lu.
- **Combien de fois `sub_821D7AE0`/`sub_821D7CD0` eux-mêmes sont
  réellement entrés** sur la durée complète (r416 n'en avait capté que
  2 sur une fenêtre de 120s, en contradiction apparente avec les 292
  déclenchements de `sub_82331E78` sur seulement 5s ici) — cet écart
  n'est PAS résolu : soit un artefact du script r425 (un message
  `Thread-specific breakpoint... deleted` avait été noté sans être
  investigué), soit une différence réelle de fréquence entre ces
  fonctions et `sub_82331E78`. Nommé pour le cycle suivant si jugé
  pertinent.
- Aucun correctif proposé ni appliqué ce cycle.

## Décisions prises

- Corriger explicitement r416 plutôt que republier une simple suite —
  même discipline que r403/r404/r406/r408/r409/r410 sur elles-mêmes
  (précédent CLAUDE.md : corriger prédécesseurs par nom et numéro de
  cycle).
- Ne pas creuser l'écart de comptage `sub_821D7AE0`/`sub_82331E78`
  sans capture supplémentaire — nommé, pas deviné (précédent
  r1111/r1113).

## Gate

Aucune source de production éditée ce cycle.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées dans le commit de ce cycle)
`ctest` relancé en entier.

## Named for r418

Lire `sub_821D7AE0`/`sub_821D7CD0` (le corps direct de la boucle par
image, jamais lu jusqu'ici) pour localiser où/si un present/swapchain
serait censé incrémenter `presented_frames`, et déterminer si ce
compteur est gaté par l'absence d'une vraie surface d'affichage dans
ce harnais de sonde sans fenêtre.

## Files

`recompilation/ace-combat-6-retail/artifacts/retail-us-native-r404-bucket17-writer/r426_throw_site.gdb/.log`,
`r427_wait_args.gdb/.log`.

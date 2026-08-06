# AC6 — pont de ressource vers la première mission

Date : 2026-08-01 (Europe/Paris)

## Résultat

Le chemin natif atteint dynamiquement : titre, création des données globales,
sélection `FILE 01`, création des données de campagne, menu principal,
`Campaign`, `New Game`, difficulté `Normal`, commandes `Normal`, langue anglaise
et cinématique d'introduction de campagne.

La sortie de cette cinématique par `Start` et sa fin naturelle échouaient au
même endroit. Le registre de ressources fournissait au constructeur
`0x8213AD60` un membre `NTXR` isolé ; le constructeur demandait ensuite le
sous-enregistrement 1, recevait zéro et le déréférençait.

Un pont natif qualifié PAL est maintenant implémenté. Il ne modifie ni le code
généré ni le registre invité : pour l'unique appelant `0x8218F6DC`, la vtable
`0x82067B14`, la clé `0x5979A7AA` et la feuille de taille `0xEB000`, il valide
en mémoire le conteneur `FHM` parent, ses 5 membres, l'identité offset/taille de
la feuille et la présence du membre 1, puis transmet le parent au constructeur.
Tout écart laisse le comportement invité inchangé.

Ce correctif est construit et couvert par tests, mais il n'est pas encore
validé dynamiquement jusqu'au gameplay : trois relances se sont figées avant le
titre et n'ont jamais exécuté le pont. La cible n'est donc pas déclarée
jouable.

## Identités et preuves

- cible : Ace Combat 6 PAL, Xbox 360, PowerPC big-endian ;
- XEX `default.xex` :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- exécutable natif final :
  `9af9397caeb256f579629abe39b62473aeb5f1ae68d23a68800c908d257ed1ca` ;
- trace dynamique source :
  `reports/logs/cycle-457-campaign-nodes/ac6recomp.log` ;
- crash naturel indépendant du bouton :
  `reports/logs/cycle-457-campaign-natural-completion/ac6recomp.log` ;
- manifeste statique : entrée 747, chemin `0.4`, offset `0x60000`, taille
  `0xEB000`, parent taille `0x14B000` ; confiance `cross-match` ;
- résolution dynamique du registre, de l'appelant et du déréférencement nul :
  confiance `dynamic` ;
- disposition FHM et garde du pont : confiance `confirmed` par manifeste,
  format et test natif.

## Changements de cette tranche

- `src/ac6_campaign_resource_bridge.h` : lecteur FHM borné et validation de la
  relation parent/feuille ;
- `src/ac6_backend_fixes/ac6_campaign_resource_bridge_test.cpp` : cas positif
  PAL et rejets magic/taille/membre requis ;
- `src/ac6_backend_fixes/ac6_ui_input_dispatch_probe.cpp` : override étroit de
  `0x8213AD60`, sondes temporaires de registre retirées ;
- `CMakeLists.txt` : ajout du test au corpus AC6 ;
- outils PPC locaux `powerpc-none-elf-ld` et `powerpc-none-elf-nm` : bit
  exécutable restauré pour rendre le corpus PPC lançable.

Les corrections antérieures de la même phase restent dans l'arbre : résolution
720p, accès POSIX lecture/écriture, pont des dialogues de sauvegarde,
symbolisation POSIX et retrait de trois entrées mid-function invalides du
codegen.

## Validation exécutée

- build final `ac6recomp` et test du pont : PASS ; action
  `164e910c0c34519a53c1427e8b82c3a236a4e73fe918457bdffacc75e1deee0b` ;
- tests ciblés AC6 : 6/6 PASS ; JUnit
  `reports/logs/cycle-457-campaign-parent-bridge-targeted-tests.xml` ;
- corpus PPC : 1457/1458 PASS ; un échec préexistant reproductible sur
  `ppc.test_vpkd3d128_float16_4_invalid_0` ;
- corpus complet : 1619 tests, 1609 PASS, 4 SKIP, 6 FAIL ; les cinq autres
  échecs sont les baselines `chrono` (4) et `TemplateRegistry` (1) ; JUnit
  `reports/logs/cycle-457-campaign-parent-bridge-full-ctest-v2.xml` ;
- `git diff --check` : PASS ;
- nettoyage final : aucun `ac6recomp`, `ac6-run.sh` ou Xvfb privé restant ;
  `/dev/shm` à 12 KiB utilisés sur 61 GiB.

## Runs interactifs après correction

- `...parent-bridge-v2` : dialogues de sauvegarde revalidés, mais `Left` est
  arrivé pendant le chargement et n'a pas sélectionné `YES` au second dialogue ;
- `...parent-bridge-v3` : gel avant titre sur le logo Bandai Namco ;
- `...parent-bridge-v4` : gel avant titre sur la scène `PRODUCED BY...`, malgré
  impulsions `Start` ;
- `...parent-bridge-v5` : moins de 30 présentations au démarrage, avant toute
  injection d'entrée.

Ces trois derniers runs établissent une borne `runtime-blocked` distincte du
crash de ressource de campagne. Ils ne valident ni n'invalident le pont, car
l'appel qualifié n'est jamais atteint.

## Risques et prochaine borne

1. Stabiliser ou contourner de façon reproductible le gel de l'introduction de
   démarrage, sans répéter les mêmes impulsions `Start`/`A`.
2. Rejouer le profil vierge jusqu'à la cinématique de campagne, envoyer `Start`
   puis exiger le log `[ac6-campaign-resource-bridge]` et une capture vivante
   après la transition.
3. Traverser les écrans briefing/appareil jusqu'à une image avec HUD et contrôle
   avion ; seulement alors qualifier la première mission de jouable.
4. Corriger séparément le dialogue `PLEASE WAIT` qui affiche la chaîne d'atlas
   `ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789`. Ce défaut visuel est confirmé ; il
   n'est pas le texte de secours natif `M70000_122` et ne bloque pas la route.


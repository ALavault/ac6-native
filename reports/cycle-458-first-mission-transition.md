# AC6 cycle 458 — transition campagne stabilisée, écran noir vivant

Date : 2026-08-01  
Cible : AC6 PAL, base image `0x82000000`, XEX SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

## Résultat

Le crash natif après la cinématique d'introduction est corrigé sans modifier le
code recompilé généré. Une exécution native complète atteint la transition,
accepte les deux ponts bornés, puis continue à présenter des images jusqu'au
timeout. La nouvelle frontière est un écran noir stable : le processus, le GPU
et le polling XAM restent vivants, mais ni `A` ni `Start` ne provoquent de
transition visible. La première mission et son HUD ne sont donc pas encore
atteints.

## Cause et corrections

### Conteneur de campagne

À l'appel PAL `0x8218F6DC -> sub_8213AD60`, l'objet registre de vtable
`0x82067B14`, clé `0x5979A7AA`, expose à `0xB8D30000` le wrapper FHM décodé :
un membre à `+0x1000`, lui-même FHM de taille `0xE2000` et sept membres. La
taille registre `0xEB000` inclut le padding. Le constructeur attend le FHM
intérieur, pas le wrapper.

Le pont `InspectCampaignWrapper` valide les deux répertoires et les membres
immédiatement consommés, puis remplace `r4` par `0xB8D31000` uniquement pour le
tuple appelant/vtable/clé/objet PAL exact. Confiance : **dynamic** pour les
valeurs vivantes, **confirmed** pour la garde et ses tests.

### Répertoire NTXR vide

Le membre 3 intérieur, `0xB8D6F000`, est un NTXR valide correspondant à
l'entrée retail 190/209. Son répertoire byte-swappé à `+0x100` est vide ;
`sub_82234DD0` renvoie donc null. `sub_821C1130` lit ensuite le compteur à
`null+2`, comportement toléré par la page basse retail mais refusé par la
protection mémoire hôte.

La table virtuelle statique confirme le service `0x826A0708 -> 0x820674D8`,
avec les slots `+24/+28/+32/+36` égaux à
`0x821C1748/0x821C1960/0x821C1130/0x821C1340`. Le second pont ne renvoie zéro
que pour l'appelant `0x821C1374`, ce service, cette vtable et le profil NTXR
vide exact. Confiance : **confirmed** pour la table et la garde,
**cross-match** pour l'identité retail de la ressource.

## Validation dynamique

- Run v15, action `29c39446a1ec887c717d609303c30ef367f93aaee97c0caad49d7ab976559136` :
  les marqueurs `[ac6-campaign-resource-bridge]` et
  `[ac6-mission-record-walker] ... result=0` sont acceptés ; au moins 120
  présentations suivent sans crash.
- La capture `cycle-458-first-mission-v15/step-39-post-campaign-intro.png` est
  noire. L'état reste identique pendant environ 40 secondes à 60 Hz.
- Run v16 : le guest observe `A` (`buttons=0x1000`), sans delta d'état ou de
  pixels.
- Run v18 : `Start` est observé, sans delta ; la capture
  `step-42-post-black-start.png` reste noire. Aucun `REX_FATAL` ni signal hôte.
- Binaire effectivement validé par v15 : SHA-256
  `c2c3d4c02a04a6597b399d9229fd6b2f611a24579011e462bc58bed5055cb6af`.
  Le rebuild final non rejoué est
  `33eab4ff5340f6099e91ecb4d2f4c28f92d91815b42136fb58f3789130b2d6d3`.

Les copies promues des artefacts sont enregistrées dans
`reports/logs/cycle-458-first-mission-v15` et `-v18`; leurs empreintes
d'action restent dans `reports/cycle-458-first-mission-run-once.json`.

## Validation native

- Tests AC6 ciblés : **6/6 pass**.
- CTest complet : **1609 pass, 4 skip, 6 échecs de référence sur 1619**.
  Les six échecs inchangés sont quatre tests chrono, TemplateRegistry et
  `vpkd3d128_float16_4_invalid_0`.
- `git diff --check` et `bash -n` passent pour les scripts touchés.
- Aucun processus AC6/Xenia/harness orphelin après les runs.

Journal JUnit : `reports/logs/cycle-458-first-mission-full-ctest.xml`.

## Frontière suivante

Déterminer pourquoi le flux post-transition présente uniquement du noir malgré
un guest et un canal d'entrée vivants. La prochaine instrumentation doit
qualifier le propriétaire d'état/mode de campagne et la complétion des
ressources immédiatement après `sub_821C1130`, puis comparer soumissions GPU et
état de scène avant/après la transition. Ne pas répéter `A` ou `Start` : leurs
deux hypothèses sont invalidées.

Défauts adjacents non résolus : mismatch de résolution et écran de dialogue
`PLEASE WAIT` affichant l'atlas de police.

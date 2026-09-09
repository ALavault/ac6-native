# AC6 retail NTSC-U/J — r480 — piste A (plan approuvé) : une seule pression `space` après le boot n'atteint toujours PAS les trois nuanceurs cibles ; l'arrêt propre via `xdotool windowclose` fonctionne mais ne produit aucun cache `.xpso`/`.xsh` persistant

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Oracle utilisé (décision utilisateur explicite : une seule tentative
bornée avec entrée synthétique minimale, autorisée en réponse à la
question posée après r479).

## Contexte

Nommé par r479, piste 1 (avec décision utilisateur explicite « une
seule tentative bornée, puis arrêt quel que soit le résultat ») :
répéter le lancement direct de l'oracle-hybride avec une entrée
synthétique minimale (une pression `space`, mimant `run_gate.py`'s
propre convention de confirmation) quelques secondes après le boot,
pour voir si le jeu de nuanceurs cible (`09dd1c7cddae1141`,
`57b8e5f14b93cff4`, `4dd456c4ea0923c1`, r478) apparaît après un
premier écran de confirmation.

## Établi — mécanisme d'entrée réutilisé depuis `run_gate.py`/`tools/ac6-oracle-run.py`

`run_gate.py` charge dynamiquement `tools/ac6-oracle-run.py` (racine
du dépôt) comme moteur d'exécution de route (`load_runner()`,
`RUNNER_PATH`) — sa méthode `input_edge()` envoie `xdotool keydown
<valeur>` puis, après une tenue configurable, `xdotool keyup <valeur>`,
après `xdotool search --classname ac6recomp` +
`windowactivate`/`windowfocus`. Toutes les étapes `("key", "space",
"0.6")` de `run_gate.py` (confirmations de hangar/carte/déploiement,
r470) utilisent cette même mécanique. Reproduite ici à l'identique,
en dehors de `run_gate.py`, sur un lancement direct du binaire déjà
compilé (`install/ntsc-uj/bin/ac6recomp`), sous un nouveau Xvfb
dédié (`:191`).

## Établi — le boot est significativement plus lent que prévu, confirmant (sans l'expliquer) l'observation de r479

Le fichier `--log_file` n'a été créé qu'après clôture propre du
processus — **aucune ligne de journal exploitable pendant toute la
durée du run** (contrairement à tous les runs `run_gate.py` de r463-
r479, qui produisent un journal dès les premières secondes). Le
processus est resté actif, utilisant 145-238 % CPU en continu (donc
non bloqué/gelé), pendant **plus de 700 secondes réelles** avant que
l'arrêt propre soit déclenché manuellement — un ordre de grandeur
au-delà de toute durée de route jamais utilisée par cette campagne
pour du contenu de démarrage précoce (le sondage natif atteint son
contenu cible en moins de 30 s, r475). Ceci confirme, sans
l'expliquer davantage (hors périmètre de ce cycle, comme r479
l'avait déjà noté), que le produit oracle-hybride a un cadencement de
démarrage substantiellement différent du produit natif pour cette
configuration de lancement précise.

## Établi — la pression `space` unique n'a pas changé l'écran atteint

`xdotool keydown space` / `sleep 0.6` / `xdotool keyup space`,
envoyée ~6 s après le lancement (avant que la fenêtre du jeu ne soit
nécessairement dans un état interactif, étant donné le boot lent
ci-dessus). Après clôture propre (`xdotool windowclose`, qui A
fonctionné — le processus s'est terminé proprement en ~20 s après
l'envoi), le dump de nuanceurs contient **exactement les mêmes 4
nuanceurs vertex que le lancement zéro-entrée de r479**
(`0a6d1dd7767fdf27`, `472913f460d4b446`, `bbaada3605b82c5a`,
`c049a8c9e556f129`), plus 3 nuanceurs fragment. **Aucun des trois
nuanceurs cibles n'apparaît.** La pression `space`, à ce moment précis
du démarrage, n'a donc pas fait progresser le jeu au-delà de l'écran
déjà atteint par r479 — soit parce que la fenêtre n'était pas encore
interactive à 6 s (le boot anormalement lent ci-dessus rend cette
hypothèse plausible), soit parce que cet écran ne répond pas à `space`.

## Établi — nouveau fait : l'arrêt propre ne produit PAS de cache persistant `.xpso`/`.xsh`

Contrairement à l'attente (r479 avait supposé que ces fichiers ne
sont écrits qu'à un arrêt propre, pas sur `SIGKILL`), **`xdotool
windowclose` a produit un arrêt propre réel** (processus terminé en
~20 s, fichiers `.ucode.bin`/`.ucode` du dump de nuanceurs bien
présents) **mais aucun fichier `.xpso`/`.xsh`** n'a été trouvé où que
ce soit sous le répertoire de travail. Soit ces fichiers nécessitent
un événement de création de pipeline réel (jamais atteint puisque le
jeu reste sur un écran de logo/menu statique tout du long), soit un
autre déclencheur d'écriture que la simple fermeture de fenêtre — non
déterminé, hors périmètre de cette tentative unique.

## Non établi

- **La cause du boot anormalement lent** (700 s+ vs. quelques secondes
  pour un `run_gate.py` normal) — non investiguée, comme déjà noté par
  r479 pour son propre run plus court.
- **Si une entrée envoyée plus tard** (une fois la fenêtre réellement
  interactive, à déterminer par un signal observable plutôt qu'un délai
  fixe) atteindrait l'écran cible — non testé, hors budget de cette
  tentative unique.
- **Ce qui déclenche réellement l'écriture `.xpso`/`.xsh`** — non
  déterminé.

## Décisions prises

- **Respect strict de la limite « une seule tentative »** actée par
  l'utilisateur après r479 : aucune variation de timing, de séquence
  d'entrée, ou de mécanisme de lancement n'a été tentée au-delà de
  celle décrite ci-dessus, malgré le boot anormalement lent qui aurait
  pu justifier un ajustement immédiat.
- **Ne pas modifier `run_gate.py`** (l'option de découplage
  `--dump_shaders`/route scellée nommée par r479) — hors périmètre de
  cette tentative, qui ciblait spécifiquement le lancement direct avec
  entrée minimale.
- Nettoyage complet : processus `ac6recomp` arrêté proprement, `Xvfb
  :191` terminé, répertoire de travail temporaire supprimé après
  extraction des résultats.

## Recommandation explicite pour la suite

**Piste A devrait être mise en pause ici.** Cinq cycles consécutifs
(r476-r480) ont progressivement éliminé chaque hypothèse peu coûteuse
(durée de route, lancement direct sans entrée, lancement direct avec
une entrée minimale à un instant fixe) sans atteindre les trois états
de tirage cibles, et ont révélé une divergence de cadencement de
démarrage entre les deux produits qui n'est elle-même pas comprise.
Les deux options restantes nommées par r479 — répéter avec un timing
d'entrée différent, ou découpler `--dump_shaders` de
`run_gate.py`'s route scellée — sont chacune un travail réel
distinct, pas une variation bon marché de plus. Continuer à itérer
sur cette piste précise sans nouvelle information qualitative
(typiquement, comprendre POURQUOI le boot est si lent) risque de
répéter le même motif de rendements décroissants. Les pistes B
(committer l'arriéré natif stabilisé) et C (garde-fou HUD) du plan
approuvé restent disponibles, indépendantes, et non touchées par ce
blocage.

## Gate

**Aucune source de production modifiée ce cycle** — lancement direct
du binaire déjà compilé, aucune reconstruction, aucun fichier
`native/` touché.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
`ctest` natif et racine inchangés (profil `native` non touché, aucune
vérification supplémentaire nécessaire).

## Named for r481

Piste A en pause (recommandation explicite ci-dessus, décision
utilisateur à confirmer). Reste ouvert, indépendant : Piste B
(committer l'arriéré natif stabilisé r433-r479) et Piste C
(garde-fou HUD) du plan approuvé — toutes deux prêtes à démarrer sans
dépendre d'une résolution de Piste A.

## Files

Aucun artefact gitignoré nouveau conservé (répertoire de travail
temporaire sous `/fastdata/lavaulta/tmp/ac6-r480-input/` supprimé
après extraction des résultats).

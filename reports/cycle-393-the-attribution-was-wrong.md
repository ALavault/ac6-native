# Cycle 393 — l'attribution était fausse : `03514000` dessine OUI/NON, et il rend

## 1. Deux verrous levés d'un coup

**Détecteur d'écran.** La navigation échouait parce que le pilote ne savait pas
sur quel écran il se trouvait ; « l'image a-t-elle changé ? » n'est pas un
détecteur d'état — une cinématique change à chaque trame, ce qui a fait croire à
deux tentatives (cycles 391, 392) qu'elles progressaient alors qu'elles
restaient dans l'introduction.

`tools/ac6-detect-screen.py` compare la bande des boutons OUI/NON à un gabarit
tiré d'une capture de référence. Validation :

```
capture de référence de l'écran de sauvegarde : save-screen 0.0
écran-titre                                   : other 72.8
cinématique d'introduction                    : other 71.1
```

Séparation nette. Piloté par ce détecteur — alterner Start et A, tester après
chaque appui — l'écran de sauvegarde est atteint **à l'itération 7**, de façon
reproductible, là où cinq exécutions à horaires fixes avaient échoué.

## 2. Le test enfin exécuté

`--ac6_skip_texture_base=0x03514000`, omission déclenchée 4 fois, écran de
sauvegarde atteint et capturé.

**Résultat : les cadres des boutons subsistent, mais « YES » et « NO » ont
disparu.**

## 3. Ce que cela renverse

`0x03514000` **dessine le texte OUI/NON**. Il s'échantillonne donc
**correctement** : c'est précisément parce qu'il rend que son omission efface
ces mots.

La mise en garde du cycle 385 est **confirmée**, et l'attribution posée au
cycle 361 — « lots multi-quads = texte manquant » — est **fausse** au moins pour
cette texture. Le lot de 20 sommets (5 quads) est bien `YES` + `NO`, cinq
caractères, cinq quads, visibles à l'écran depuis le début.

Conséquence sur les cycles 362 à 384 : les mesures restent exactes en tant que
faits sur `03514000` et `028B7000`, mais leur **prémisse** — que ces deux
textures portent le texte manquant — tombe pour l'une des deux. Vingt-sept
éliminations reposaient dessus.

Il faut donc reprendre : **quelle passe dessine le navigateur GAME DATA et
« Load file 01? »**, présents chez l'oracle (cycle 342) et absents chez nous ?
Ce n'est pas la passe étudiée depuis le cycle 347.

## 4. Ce que la méthode retient

Une prémisse jamais mesurée oriente le travail aussi efficacement qu'une
prémisse fausse. Celle-ci a tenu trente-deux cycles parce qu'elle était
plausible et qu'aucune mesure ne la visait. Le seul instrument qui pouvait la
tester — relier un dessin à des pixels — n'a été construit qu'au cycle 386,
après qu'elle eut servi de socle.

**Construire d'abord la mesure qui pourrait réfuter la prémisse, pas celle qui
la prolonge.**

## 5. État

P1.3 reste bloquée, et la cible de l'enquête change. La première mission ne se
joue pas. `recompiler-generated` n'est pas `verified`.

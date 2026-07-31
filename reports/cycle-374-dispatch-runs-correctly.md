# Cycle 374 — la dispatch de chargement s'exécute, et correctement

## 1. Mesure

Groupes de travail émis pour les sept textures de la passe :

| base | size_blocks | groupes | attendu | rend |
|---|---|---|---|---|
| 028B2000 | 16x16 | 2x1x1 | 2x1 | oui |
| 028D0000 | 16x180 | 2x6x1 | 2x6 | oui |
| 028E9000 | 240x66 | 30x3x1 | 30x3 | oui |
| 0294A000 | 52x12 | 7x1x1 | 7x1 | oui |
| 02953000 | 56x16 | 7x1x1 | 7x1 | oui |
| **028B7000** | 80x45 | **10x2x1** | 10x2 | **non** |
| **03514000** | 64x64 | **8x2x1** | 8x2 | **non** |

La règle `groupes = ceil(x/8) x ceil(y/32)` est vérifiée **pour les sept**, sans
exception. Les deux textures fautives reçoivent donc une dispatch émise, avec un
volume de travail exact et proportionnel à leur taille.

## 2. Ce que cela élimine

**« La copie ne s'exécute pas »** — le dernier candidat nommé au cycle 373 — est
**réfuté**. La chaîne complète est désormais mesurée correcte de bout en bout :

données sources presentes -> chargement réussi -> load shader choisi ->
disposition invitée calculée -> disposition hôte concordante -> dispatch émise
avec le bon nombre de groupes -> vue d'image réelle liée -> constante de fetch
identique -> passe qui peint l'écran.

**Et l'échantillon est nul.** Vingt et une causes éliminées.

## 3. Ce qui n'a jamais été observé

Tout ce qui a été vérifié est **statique** : des paramètres, des tailles, des
adresses, des comptes. Rien n'a porté sur la **synchronisation** — c'est-à-dire
sur l'ordre réel d'exécution entre l'écriture de l'image par la dispatch et sa
lecture par le dessin.

C'est le candidat qui reste, et il est cohérent avec l'ensemble :

- une **barrière manquante ou mal placée** entre la dispatch de chargement et
  l'échantillonnage ferait lire une image non encore écrite — sans qu'aucun
  paramètre ne diffère ;
- cela expliquerait pourquoi seules **certaines** textures échouent : celles
  chargées tardivement, au moment même où la passe les échantillonne, tandis que
  celles chargées plus tôt ont eu le temps d'être écrites.

Cette dernière conséquence est testable : les deux fautives ont été chargées aux
rangs **#2310** et **#2321** (cycle 365), soit très tard. L'ordre de chargement
des cinq fonctionnelles n'a jamais été relevé.

## 4. Front suivant

Comparer le rang de chargement des sept textures. Si les cinq qui s'affichent
sont chargées tôt et les deux fautives tard — au moment où la passe les utilise
— la synchronisation devient la cause probable, et la barrière entre dispatch de
chargement et échantillonnage est l'endroit à examiner.

La donnée est déjà journalisée (`[ac6-texload] load #N`) ; il suffit de la lire
pour les sept.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

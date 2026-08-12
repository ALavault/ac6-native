# Cycle 1545 — trace native du replay à 60 Hz

## Résultat

`ac6-native replay --trace` écrit désormais un échantillon de trace pour chaque
frame du replay natif. Une séquence de `N` frames produit exactement `N`
échantillons et `5N` événements de domaine.

La restriction historique imposant un nombre pair de frames et deux entrées
identiques par paire est supprimée. Les replays issus de l'ancien constructeur
30 vers 60 Hz restent lisibles : leurs deux frames identiques successives sont
simplement observées comme deux échantillons natifs distincts.

## Garde de non-régression

`ac6-retail-replay-trace-cadence` soumet trois frames successives, différentes
et en nombre impair à la même fonction d'émission utilisée par la commande. Le
test exige les ticks `1, 2, 3`, les trois entrées originales, quinze événements
et le rejet sans écriture d'un index qui ferait déborder le tick.

Cette correction retire un second rééchantillonnage implicite du chemin
replay projeté. La cadence source et son éventuel maintien d'ordre zéro doivent
être scellés avant la production d'`AC6RTPLY`; la commande native ne les devine
plus et simule chaque frame à 60 Hz.

La comparaison à l'oracle 30 Hz est maintenant une dérivation explicite du
reçu de projection. Pour un reçu `hold=2`, le scelleur recoupe d'abord chaque
input brut avec les frames `AC6RTPLY`, puis sélectionne les snapshots natifs
aux ticks 2, 4, … ; pour `hold=1`, il conserve chaque tick. Le plan
d'observation, le reçu, sa filiation et son SHA sont inscrits dans les deux
traces comparées. Le contrat historique 30/2 reste lisible sans reçu, mais
aucune nouvelle trace 60/1 ne peut emprunter cette voie legacy.

# Checkpoint 2 — stratégie de preuve statique d'abord

## Règle

Une tranche du checkpoint 2 commence par les octets retail qualifiés, les
frontières canoniques, l'ABI et le dataflow statique. Elle est ensuite contrôlée
par un port C++ manuscrit, des entrées/sorties bit-à-bit et le corpus des quinze
missions lorsque le lecteur est partagé.

La micro-exécution n'est requise que si plusieurs sémantiques Xenon compatibles
avec l'analyse statique restent possibles et que leur différence affecte le
résultat retail. Son rapport doit nommer cette ambiguïté, exécuter les octets
retail sans sémantique affirmée, stub ou pont de registres caché, et conserver
la frontière non couverte lorsqu'un contrôle honnête est impossible.

## Hiérarchie appliquée

1. identité du module, projet Ghidra canonique, adresse et mots retail ;
2. bornes de format, contrôle positif et rejets déterministes ;
3. dataflow/ABI et preuve statique de la transformation ;
4. tests natifs bit-à-bit et propriétés métamorphiques ;
5. corpus retail des quinze missions et traces déterministes de niveau jeu ;
6. micro-exécution ciblée de l'ambiguïté résiduelle seulement.

Une capture Xenia ou du code généré reste un oracle de comparaison et ne peut
pas remplacer le port natif, ses tests, ni les preuves de provenance.

## Application actuelle

Les inventaires Scene/TCAM, les plages ASF et le corpus des producteurs de
compteurs relèvent des niveaux 1 à 5 et ne nécessitent pas de micro-exécution.
Les estimations `vrefp`/`vrsqrtefp`, certains arrondis mathématiques retail et
les dispatchs indirects non résolus restent des candidats légitimes au niveau
6. Une simple copie VMX de mots bruts n'en est pas un si son chemin et ses
bornes sont établis sur le corpus.

Cette politique réduit le nombre de contrôles instructionnels sans substituer
des mathématiques hôte aux résultats Xenon et sans déclarer couvert un cas
ambigu.

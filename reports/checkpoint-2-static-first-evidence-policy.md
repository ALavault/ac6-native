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

## Bring-up provisoire avec RexGlue

Pour accélérer la verticale M01, une sémantique atteinte et effectivement
implémentée par une révision RexGlue épinglée peut désormais être utilisée
comme référence de travail `provisional-rexglue` lorsqu'aucune preuve contraire
n'est connue. Elle autorise le port C++ manuscrit, les tests d'intégration et
les replays diagnostiques sans exiger immédiatement une micro-exécution.

Cette présomption ne ferme aucune lane et ne constitue pas une preuve de
fidélité retail. Avant JV/JP/JG ou publication, chaque dépendance provisoire
réellement atteinte par M01 doit devenir `retail-qualified`, ou être démontrée
hors du cône exécuté. Une révision différente exige un diff sémantique.

Une approximation, un stub, une instruction absente ou une contradiction déjà
connue est `divergent` et ne profite jamais de la présomption. Le registre
machine-readable `analysis/rexglue-semantic-trust-v1.json` conserve les règles,
les pins et les premières exclusions (`dcbst`, estimations réciproques,
réservations atomiques et barrières mémoire). L'audit global interdit leur
promotion accidentelle en preuve de gate.

## Application actuelle

Les inventaires Scene/TCAM, les plages ASF et le corpus des producteurs de
compteurs relèvent des niveaux 1 à 5 et ne nécessitent pas de micro-exécution.
Les estimations `frsqrte`/`vrefp`/`vrsqrtefp`, certains arrondis mathématiques
retail et les dispatchs indirects non résolus restent des candidats légitimes
au niveau 6. Les implémentations RexGlue actuelles des trois estimations
emploient des mathématiques hôte exactes et sont donc explicitement
`divergent`. Une simple copie VMX de mots bruts n'est pas un candidat à la
micro-exécution si son chemin et ses bornes sont établis sur le corpus.

Cette politique réduit le nombre de contrôles instructionnels sans substituer
des mathématiques hôte aux résultats Xenon et sans déclarer couvert un cas
ambigu.

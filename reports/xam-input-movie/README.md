# AC6 XAM input movie v1

Le movie déterministe est implémenté à la frontière guest-visible
`XamInputGetState`. Il enregistre chaque résultat avec l’état exact de 16
octets, le résultat, user, flags, ordinal global, LR appelant, thread et tick
guest. Le replay strict retourne ces valeurs avant tout accès HID hôte et
rejette la première divergence de type, ordinal, appelant, user, flags,
nullité ou longueur.

La qualification end-to-end reste ouverte. Le bootstrap borné à 900 secondes
avec la recette Mission 01 retardée a produit 3 754 événements cohérents sans
fatal, puis s’est arrêté à l’étape 59 : la transition campagne
`state=0->1` n’a jamais été observée après le dernier état qualifié
`state40=0, selector44=0, response12=2, type28=10, result36=0`.

En conséquence, aucun replay n’est déclaré qualifié et les runs atlas/enrichi
n’ont pas été lancés. Les sorties brutes incomplètes ont été supprimées :
elles contenaient les diagnostics X11 et captures de présentation de l’ancien
harness. Le mode movie courant désactive ces captures et isole aussi le cache
hôte. Les identités, hashes, validations et le contrat du prochain essai sont
dans `qualification.json`.

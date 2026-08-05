# Cycle 967 — inventaire code machine-readable

`reports/ac6-code-reachability-inventory.json` devient l’artefact versionné
des racines de sélection, chargement, HSM, update, événements, factories
d’unités et radio. Il qualifie les contrats natifs avec module, symbole,
callers/callees, rôle et preuve de test; les racines retail non démontrées
portent `retail_status=unknown` et un gap explicite.

`tools/audit_code_reachability.py` vérifie le projet Ghidra canonique,
l’identité PAL/XEX, les racines obligatoires, les graphes bornés et le
fail-closed des inconnues. Résultat :
Résultat initial : `roots=7 native_covered=7 retail_unknown=7 entries=9`.

Le checkpoint suivant a ajouté les frontières retail Ghidra déjà qualifiées
dans `analysis/address_catalog.tsv` sans les confondre avec une couverture de
graphe : six racines sont maintenant `retail_status=partial`, une reste
inconnue faute d’adresse qualifiée.

Ce checkpoint ferme la provenance et l’absence de lacune silencieuse, mais ne
prétend pas qualifier les graphes retail manquants ni les 13 routes campagne
non qualifiées.

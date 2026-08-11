# Cycle 1516 — dispatch explicite des OrderFlagBin

## Delivered

`RetailSession` expose maintenant `apply_flag_order(index, now, random)` et
`counter(id)`. L’API sélectionne uniquement un `ScenarioFlagOrder` déjà parsé,
convertit son littéral/opération vers `SubMissionSequencer::apply`, conserve la
garde de domaine sur les opérations inconnues et laisse au scheduler le choix
du moment d’exécution.

Cette tranche ferme le raccord session → écrivain de compteurs sans inventer
le déclenchement des programmes Set/Act/Order. Elle ne transforme donc pas un
ordre présent dans le cache en progression automatique.

## Validation

Le test de session rejoue le premier ordre réel de la Mission 01, vérifie la
valeur publiée et refuse un index hors tableau. La suite complète C++ reste à
70/70 tests réussis (deux skips d’environnement : ressources frontend et
surface Vulkan). Les tests Python restent à 91 réussis et 14 sous-tests.

## Boundary retained

Le scheduler des comportements, les producteurs de compteurs issus des états
IA/combat et la règle retail qui autorise une transition de sous-mission
restent à dériver. Aucun succès de mission ou gate JV/JP/JG n’est revendiqué
par cette API seule.

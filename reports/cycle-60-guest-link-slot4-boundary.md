# AC6 — borne native de recherche de lien invité `0x821D1BE8`

Date : 2026-07-16

## Identité et preuves

- Cible : AC6 Xbox 360 ; pas Xbox One.
- Fonction : `Function_821D1BE8`, XEX `0x821D1BE8`.
- Décompilation, appelant et absence de callee statique :
  `workspaces/ace-combat-6/exports/821d1be8.json`.
- Le checker précédent a rejeté la classe C++ proposée car elle n'exprimait pas
  le slot de vtable `+0x04` et parce que les champs de noeud `+0x00/+0x04`
  n'étaient pas qualifiés :
  `workspaces/ace-combat-6/reports/logs/round1-20260715-075215-checker.json`.

## Transposition native bornée

`function_821d1be8_find_link` représente explicitement un lien invité de huit
octets : `entry_at_0` et `next_at_4`, tous deux des adresses invitées 32 bits.
La recherche conserve :

1. la garde `target != 0` ;
2. l'ordre de parcours `next_at_4` ;
3. l'exclusion des entrées nulles ;
4. la comparaison avec le retour de l'appel indirect lu à vtable `+0x04` ;
5. le retour du lien correspondant, et non de l'entrée.

Le slot est un callback explicite. Aucun nom de méthode, ABI C++ hôte, classe,
ou signification de l'entier retourné n'est inventé. Un plafond de parcours,
un lien non résolu et un callback absent sont des protections de l'adaptateur
natif ; ils ne sont pas attribués au comportement retail.

## Validation

La suite `ac6-link-821d1be8-tests` couvre le premier match, un match après une
entrée nulle, la garde zéro, un lien illisible, l'absence du callback slot 4 et
le plafond de parcours. Le corpus CTest AC6 complet passe 39/39 après la
tranche. L'installation racine produit les exécutables existants sans créer
`bin/bin`.

## Limite restante

Cette primitive ne résout pas les cibles indirectes de la traversée de frame,
ne relie pas son appelant `0x821D2FC0` à une mission, et n'établit ni activation
de scène ni vol. Cette frontière reste donc `needs-dynamic-evidence` pour la
chaîne de consommation de mission.

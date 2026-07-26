# AC6 — borne récursive de recherche d’entrée invitée `0x821D2FC0`

Date : 2026-07-16

## Preuves statiques

- Cible : AC6 Xbox 360 ; pas Xbox One.
- XEX : `0x821D2FC0`.
- Décompilation : `workspaces/ace-combat-6/exports/821d2fc0.json`.
- Assembleur PowerPC de contrôle :
  `workspaces/ace-combat-6/reports/821d2fc0.asm`.

L'assembleur qualifie l'ordre suivant :

1. lire `owner +0x1c` ;
2. appeler `0x821D1BE8(target, first_link)` ;
3. retourner `matching_link +0x00` si trouvé ;
4. sinon parcourir les mêmes liens ;
5. pour chaque entrée non nulle, appeler vtable `+0x0c` ;
6. descendre récursivement seulement si son retour signé vaut exactement `1`.

## Transposition native

`function_821d2fc0_find_entry` garde la disposition invitée de l'owner
(`first_link_at_1c`) et celle des liens de `0x821D1BE8`. Les dispatchs `+0x04`
et `+0x0c` sont deux callbacks distincts. Aucun nom de méthode, type d’objet,
ni sémantique de ressource n'est ajouté.

La résolution mémoire, les liens illisibles, l'absence de callback et la
profondeur maximale échouent explicitement. Les limites de liens/profondeur
protègent l'adaptateur natif contre une chaîne invitée cyclique ; elles ne sont
pas présentées comme des règles du XEX.

## Validation

`ac6-link-821d1be8-tests` couvre le match direct, un match réellement
récursif, la garde de cible zéro, les liens illisibles, les deux callbacks
manquants et la limite de profondeur. Le corpus complet AC6 doit rester la
porte de livraison de cette tranche.

## Frontière non résolue

La fonction a de nombreux appelants et n'établit pas à elle seule le rôle des
entrées ni leur lien avec l'activation d'une mission. Elle n'identifie pas les
cibles indirectes de `0x8226ECB0`; la file AC6 conserve donc la porte
Xenia/XenonTests ciblée pour la traversée de frame.

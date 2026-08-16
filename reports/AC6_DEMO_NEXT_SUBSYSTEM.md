# AC6 — décision sur le prochain sous-système

Date : 2026-08-17

Oracle sémantique secondaire admis : `https://acecombat.fandom.com/wiki/Invasion_of_Gracemeria_(mission)` ; la page sert à ordonner les phases, jamais à remplacer une xref binaire.

## Décision

Le prochain sous-système à attaquer est :

**production des événements de dommage par les impacts et collisions**.

Ce choix ne consiste pas à prolonger artificiellement `WeaponBin`. La frontière vient justement d’être localisée : `WeaponBin` se termine sur l’acquisition, la trajectoire, la cadence et la famille ; l’interface suivante fabrique un événement `GUNDAMAGE`, `MISSILEDAMAGE`, `GROUNDDAMAGE` ou `FRAKEDAMAGE` avec une magnitude.

## Pourquoi ce front est prioritaire

Il ferme la seule lacune majeure de la chaîne combat :

`WeaponBin → projectile → impact → code + magnitude → DurableBin → durabilité → destruction`

Les deux extrémités sont déjà qualifiées :

- côté projectile, les portées, vitesses et familles sont connues ;
- côté cible, l’équation de durabilité et les multiplicateurs `DurableBin` sont connus.

Le segment restant est petit et discriminant. Le fermer permettra de remplacer dans `ac6-native` les valeurs synthétiques de dommage par un contrat retail étayé.

## Première tranche bornée

1. Partir de `demo:0x820A2698` et résoudre le vslot appelé à `demo:0x820A2C58` avec le code 1008.
2. Reconstituer la structure d’impact passée dans `r5` et l’origine de sa magnitude.
3. Inventorier les producteurs des codes :
   - 1007 `GUNDAMAGE` ;
   - 1008 `MISSILEDAMAGE` ;
   - 1009 `GROUNDDAMAGE` ;
   - 1014 `FRAKEDAMAGE`.
4. Joindre chaque producteur à :
   - classe/vtable du projectile ou collisionneur ;
   - vitesse relative et angle d’impact ;
   - effet ou table auxiliaire ;
   - magnitude finale transmise à l’événement.
5. Tester si la magnitude dépend de :
   - l’énergie cinétique ;
   - une table par famille ;
   - la cible ;
   - la difficulté/ESM ;
   - un mélange de ces facteurs.

## Critère de fermeture

Pour chaque famille de dommage, produire :

`producer class → input fields → formula → event code → magnitude → DurableBin channel`

Un nom de type « damage » n’est accepté qu’après cette formule. Une constante lue près d’un missile reste une constante lue près d’un missile, malgré tous les efforts de notre cerveau pour y voir une destinée.

## Front suivant après celui-ci

Une fois le dommage fermé, le meilleur front devient l’orchestration de mission `ActBin/OrderBin/SetBin`, afin de résoudre :

- activation des groupes Nimbus ;
- clonage/activation des chars aéroportés ;
- vagues F/A-18F, Rafale et Strigon ;
- ordre de retraite ;
- condition de franchissement de la Return Line.

La chronologie publique de Mission 01 fournira alors un oracle sémantique utile, sans remplacer les preuves de provenance binaires.

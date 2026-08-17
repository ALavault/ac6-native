# AC6 — décision sur le prochain sous-système

Date : 2026-08-17

## Front désormais fermé

La production de magnitude par les armes à projectile est fermée :

```text
WeaponBin+0x5D
+ modificateur contextuel[10]
→ configuration projectile
→ impact
→ événement compact+0x0C
→ DurableBin[event]
→ durabilité
```

Le faux producteur `0x820A2C58` est rétracté : il appartient à la décoration de texte radio.

## Nouveau sous-système prioritaire

Le prochain front est :

```text
ActBin / OrderBin / SetBin
→ activation, clonage ou changement d'état d'ObjBin
→ UnitManager / MissionManager
→ transition de mission
```

## Pourquoi ce front est maintenant prioritaire

Les mécanismes locaux d'une unité active sont suffisamment qualifiés :

- provenance `ObjBin → objet actif` ;
- IA `ManeuverBin → pitch/roll/yaw` ;
- `WeaponBin → acquisition, trajectoire, cadence et dommage` ;
- `DurableBin → perte de durabilité`.

Le principal manque est désormais l'orchestration globale de Mission 01 :

- activation des groupes Nimbus 200–208 ;
- clonage ou activation des chars aéroportés ;
- vagues F/A-18F, Rafale M et Strigon ;
- ordre de retraite ;
- condition de franchissement de la Return Line ouest ;
- changement d'ordre ou désactivation des unités encore présentes.

## Première tranche bornée

1. Retrouver le switch des tags `OrderBin` :
   `Disappear`, `Stop`, `Lead`, `Jump`, `Flag`, `Property`.
2. Relier chaque handler à une mutation exacte :
   - flag HSM ;
   - état d'activation ;
   - ordre/cible courant ;
   - entrée `SetBin` ou groupe `ObjBin`.
3. Suivre les opérations d'activation jusqu'à :
   - une factory `UnitManager` ;
   - l'activation d'un objet préconstruit ;
   - le clonage d'un template.
4. Identifier le prédicat terminal de retraite qui lit le transform du joueur et écrit l'état de mission.

## Critère de fermeture

```text
phase de mission
→ record Act/Order/Set
→ handler
→ mutation exacte
→ groupe ObjBin
→ effet UnitManager/HSM
```

La chronologie publique de *Invasion of Gracemeria* reste un oracle sémantique secondaire. Elle peut contraindre l'ordre attendu, mais ne remplace jamais la provenance binaire. Les formats propriétaires ont déjà assez de pouvoirs occultes sans leur attribuer ceux d'une page wiki.

## Fronts secondaires

En parallèle, deux petites frontières du dommage peuvent être fermées à faible coût :

- nom métier du modificateur d'identifiant 10 ;
- owners exacts des producteurs `GROUNDDAMAGE` et `FRAKEDAMAGE`.

Elles ne justifient plus de retarder l'analyse de l'orchestration de mission.

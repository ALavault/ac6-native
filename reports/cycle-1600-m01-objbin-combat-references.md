# Cycle 1600 — références combat ObjBin bornées

## Résultat

Le lecteur natif de scénario conserve désormais, pour chacun des 434 Obj de
M01, les offsets de payload résolus par le record ObjBin de 0x20 octets : data,
paramètre, manœuvres, DurableBin, trois WeaponBin et tail inconnu. Ces valeurs
sont des références structurelles bornées ; aucun champ interne de
DurableBin/WeaponBin, statistique d'arme ou lien avec le loadout n'est inféré.

Le niveau de traversée est celui du validateur indépendant qualifié pour
`0x8232F380 → 0x8232F198 → ObjBin`. Le contrôle PAL retrouve 434 records et
l'histogramme WeaponBin scellé : 152 records sans arme, 189 avec une arme, 93
avec deux, aucun avec trois. Chaque vecteur de références reste aligné avec les
scalaires et bindings modèle du même Obj.

## Régression partagée

Un cache RetailContentStore v2 complet a été reconstruit depuis les ISO PAL :
926 enregistrements, 5 424 368 676 octets, index
`cfca517e3f843169ca01fc52700472e66b86365621a922fc27a64a21ab713f85`.
Le corpus store-backed M01–M15 passe sur cet index : quinze capacités de
compteur et 3 650 producteurs OrderFlagBin restent identiques et bornés.

## Contrôles

* build : `ac6-retail-scenario-parser-tests` et
  `ac6-retail-session-tests` passent ;
* CTest PAL ciblé : parseur scénario et session, 2/2 ;
* validateur ObjBin indépendant : aucune incohérence ni différence enregistrée ;
* corpus partagé cache v2 : M01–M15, 1/1, zéro skip ;
* audit de complexité après extraction du lecteur dédié : 284 fichiers, passé ;
* suite CTest complète avec cache PAL et audio dummy : 87/87, zéro skip ;
* `git diff --check` : passé.

La lane objectifs/campagne reste ouverte. La prochaine frontière est la
sémantique interne WeaponBin/DurableBin et le raccord qualifié
loadout → définition d'arme → destruction → compteur.

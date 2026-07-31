# Cycle 432 — la branche d'attente est un **retour immédiat**

## 1. `loc_821C58CC` en entier

```
li   r3,0
addi r1,r1,192
b    __restgprlr_19        ; restauration + retour
```

Trois instructions. **C'est un retour anticipé qui ne fait rien.**

## 2. La boucle du blocage, complète

1. `sub_821CE8A8` appelle le sélecteur, reçoit 997 (`IO_PENDING`), pose
   `[obj+84] = 2` ;
2. à chaque trame, `sub_821C56F8` lit `[obj+84]`, le trouve non nul, et saute à
   `loc_821C58CC` ;
3. `loc_821C58CC` rend 0 immédiatement ;
4. l'écran se redessine à l'identique — le symptôme mesuré depuis le cycle 404.

C'est un comportement **normal** : pendant l'attente, ne rien faire et
réessayer. Le défaut n'est pas là.

## 3. Le point dur

Qui remet `[obj+84]` à 0 ?

Recherche des écritures à l'offset 84 dans le fichier : les 38 correspondances
portent sur `r1+84`, c'est-à-dire la **pile**, sauf celles de `sub_821CE8A8` sur
`r31+84`. Autrement dit, **le seul écrivain de ce champ est le lanceur
lui-même**, et il ne le remet à 0 que si l'appel au sélecteur rend autre chose
que 997.

Or le journal ne montre **qu'un seul** appel à `XamShowDeviceSelectorUI` sur
toute l'exécution (cycle 417).

## 4. Ce que cela implique — et la réserve

Si aucune autre écriture n'existe, l'écran ne peut sortir de l'état 2 qu'en
rappelant `sub_821CE8A8`, ce qu'il ne fait pas puisqu'il retourne avant.

**Réserve explicite** : ma recherche a porté sur un seul fichier généré et sur
une forme textuelle précise. Un écrivain dans un autre fichier, ou via un
registre de base différent, m'échapperait. Je ne déclare donc pas l'interblocage
prouvé.

## 5. Vérification qui trancherait, courte

Relever `[obj+84]` à l'exécution sur l'écran bloqué. S'il vaut 2 en permanence,
la boucle décrite est confirmée et le champ ne bouge jamais ; s'il varie, un
autre écrivain existe et il faut le trouver.

L'objet est atteignable : `[screen+4]`, et l'écran est l'argument de
`sub_821C56F8`.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

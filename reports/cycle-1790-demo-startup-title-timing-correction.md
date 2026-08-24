# Cycle 1790 — correction de chronologie StartUp / vrai Title

Date : 2026-08-23  
Cible : démo PAL `Default.xex`  
SHA-256 : `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`  
Projet Ghidra canonique : `ghidra-projects/ace-combat-6-demo`

## Résultat

Le gate renderer reste fermé naturellement jusqu'au logo Namco. La correction
étroite du troisième profil vertex titre `{base=8,count=4}` est validée : le
guest demande trois fenêtres de quatre sommets sur un stride de treize dwords,
et la troisième exige exactement 624 octets. Le run r3 dépasse l'ancien trap
et atteint 3407 ticks avec 1158 PRESENT, sans forcer un draw, un handle, Q1,
le scheduler ou des pixels.

Le gate frontend reste ouvert, mais l'ambiguïté temporelle est désormais
fermée. Une sonde observation-only corrèle exactement l'invocation
`0x820D18C8 -> 0x82323808` du tick 3105 :

```text
CTaskModeManager            0x18980000
manager+0x08 current task   0x2E7E0080
current task vtable         0x8201130C = CModeTaskStartUpDemoOffline
Task+0x1C CSwgManager vptr  0x82006438
Task+0x20 aggregate A       0x2E3C3AD4

Memory::factory LR          0x8232356C = child callsite
constructor parent          0x2E3CDD10 != 0
new MovieController         0x2E3CED10, vtable 0x820304D8
factory aggregate A         0x2E3C3AD4 == task aggregate A
```

La construction est donc un `MovieController` **enfant**, naturel et joint à
l'agrégat SWG de la tâche Startup courante. Ce n'est ni un controller racine
republié, ni le vrai Title, ni une transition de mode. Le pulse START au tick
3000 a été injecté pendant `CModeTaskStartUpDemoOffline`; il était trop tôt.
Cela explique pourquoi les analyses du supposé « post-START TitleUS » ne
pouvaient produire ni EndMode ni `manager+0x18`.

Le payload `raw=0x3F8` vu plus tard est exactement le record type 6 de la frame
1019 du descripteur `brandLogo`. C'est une donnée SWG sérialisée, pas une
fonction, un attribut de mode ou un signal frontend. Le prompt utilisateur
« PRESS START » arrive bien après le logo Namco.

## Sémantique qualifiée

- `0x827435F8` : cellule globale mutable contenant `CTaskModeManager*` ;
- `0x821929A8` : méthode virtuelle d'instance du manager ;
- `manager+0x08` : attribut pointeur vers la tâche courante ;
- `0x8201130C` : donnée globale constante, vtable Startup ;
- `0x820113E4` : donnée globale constante, vtable du vrai
  `CModeTaskTitleDemoOffline` ;
- `Task+0x1C` : sous-objet incorporé `CSwgManager` ;
- `CSwgManager+0x04` : attribut pointeur vers l'agrégat SWG ;
- `0x820D18C8` : méthode virtuelle/factory de l'objet `Memory` ;
- `0x82323808` : initialiseur/constructeur-like d'instance
  `swg::MovieController` ;
- `0x8232356C` : adresse de retour du callsite enfant, pas une méthode ;
- `MovieController+0x10/+0x20/+0x24/+0xD0` : attributs d'instance vérifiés.

La qualification d'objet n'arme pas le manager. La passe Ghidra canonique
confirme séparément que `0x82192CC8` est un writer interne de l'update manager
et que le seul writer du vrai Title est `0x8218A800`, conditionné par son
listener/état 2. Aucun producteur TitleUS naturel post-START n'a été trouvé.
La chaîne `menu_endMode -> listener -> Title état 2 -> manager+0x18` reste
valide uniquement si `menu_endMode` est réellement invoquée.

## Validation

- builds codegen-ON et codegen-OFF : statut 0 ;
- binaire codegen-ON :
  `8241a76bb13f9c4984295e8ba99b6c75e0c105db4ef2c4af641fff1b78710b0e` ;
- CTest : 26/27, seul le test de complexité connu échoue ; aucun refactoring
  n'est entrepris avant gameplay visible ;
- run factory : 3110 ticks, 1059 PRESENT, statut attendu `max_ticks`, 1 thread
  runnable et 22 bloqués, aucun trap ;
- replay :
  `78bb9b3c3a745f4158cbaf37f43249db05c4aba42de50bef4b7e27afa6ca482e` ;
- trailer interne reproduit :
  `b216baa805fe7f4221986e3c4c1d6660d54167c9ecb9270a60f31c4da5980ff0` ;
- `frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

L'audit incrémental de `ac6_demo_work` ajoute un bundle Xenia Edge stable,
des HIR et une piste terrain/arrondis `vfetch`, mais aucun nouveau prédicat
PRESS START, task/vtable ou frontend natif. Les objets
`PrimitiveProcessor`/`TraceWriter` qu'il expose sont hôtes Xenia, pas des
classes Project Aces.

## Prochain gate

La prochaine ambiguïté causale est strictement temporelle et typée : quand la
baseline renderer courante remplace-t-elle naturellement Startup par une
instance portant la vtable Title `0x820113E4` ?

Après épuisement des preuves statiques disponibles, un seul cold run est
autorisé avec `AC6_DEMO_WATCH_MODE_STATE=1`. Il doit :

1. observer dans ce même run `manager+0x08` passer à une instance dont la
   vtable vaut `0x820113E4` ;
2. n'accepter l'expérience START que si ce switch précède le tick d'entrée ;
3. injecter START à 7000 et le relâcher à 7001 ;
4. arrêter à la première mutation persistante du Title, au writer
   `manager+0x18`, au remplacement de tâche ou à la borne 7400 ;
5. déclarer `NO_GO_TIMING` si la vtable Title n'est pas active avant START.

Le tick 7000 est un choix de fenêtre, pas une preuve de prompt. Aucun état
guest ne sera écrit, aucune factory appelée et aucun événement SWG forcé par
l'hôte. Le gate frontend exigera encore la paire état guest persistant + frame
visible post-transition.

## Preuves

- `artifacts/goal-playable/title-vertex-window-8-20260823/RESULT.md`
- `artifacts/goal-playable/title-terminal-trace-runtime-20260823-r3/RESULT.md`
- `artifacts/goal-playable/title-moviecontroller-factory-join-20260823/RESULT.md`
- `artifacts/goal-playable/title-moviecontroller-factory-join-runtime-20260823/RESULT.md`
- `artifacts/goal-playable/startup-title-timing-reconcile-20260823/RESULT.md`
- `artifacts/goal-playable/manager-arm-predicate-static-20260823/RESULT.md`
- `artifacts/goal-playable/title-listener-dispatch-static2-20260823/RESULT.md`
- `artifacts/goal-playable/ac6-demo-work-delta3-20260823/RESULT.md`

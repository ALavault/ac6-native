# AC6 PAL démo — correction du sélecteur SWG post-START, cycle 1788

Verdict : **SWG-NAMED-AMBIGUITY-CLOSED / FRONTEND-OPEN**,
`supported=false`.

## Qualification

- Cible : démo PAL `Default.xex`, SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.
- Projet canonique : `ghidra-projects/ace-combat-6-demo`, module
  `Default.xex`, Xenon PPC big-endian.
- Passe statique et réanalyse de reçus qualifiés ; aucun runtime, oracle,
  shim, état guest forcé ou changement produit.
- Les prototypes retail restent auxiliaires. Aucun type, constructeur,
  layout, slot, attribut ou nom `CModeTask*DemoOffline` n'en est transféré.

## Correction causale

Le cadrage du cycle 1787 « `M102=0` ou lookup `0x0B` choisit ou saute
`EndMode` » est réfuté. Au tick 3001, le bloc
`GetCurrent*/SendMsgI("M102")/0x0B` (`0x2DCB2430`) et l'item audio
`OnVoice2D` (`0x2DCB264C`) sont deux entrées indépendantes déjà placées dans
`MovieMemory`. Le lookup `0x0B` retourne simplement `NULL` dans
`0x820DFFB8`; `M102` produit un `Integer(0)` via des listeners actifs qui
sont des stubs. Aucun des deux chemins n'écrit une frame, un curseur ou la
queue.

Le vrai producteur de statement est le handler type 6 `0x82322A80` : il lit
`payload=[owner+0xF8]+4` puis appelle le slot `MovieMemory+0x24`. Pour mettre
`EndMode` en queue, il faudrait une commande exacte
`{type=6,payload=0x00000E04}`. Cette commande n'est jamais ajoutée sur la
route START qualifiée.

## Mécanisme START prouvé

Dans la fenêtre START qualifiée, le chemin d'initialisation republie la plage
active du `swg::MovieController` dominant :

```text
avant : [0x2DD69710, 0x2DD69720)  # deux entrées
après : [0x2DD6A854, 0x2DD6A85C)  # une entrée type 4
```

Le même LR `0x82323860` prouve la republication mais ne distingue pas, seul,
nouvelle instance ou adresse réutilisée, ni changement de storage `B`, de
descriptor `D`, ou des deux. La nouvelle entrée ne peut plus sélectionner le
handler type 6. Elle n'est
toutefois pas morte :

```text
outer:   type 4 -> 0x823239F0 -> callback secondaire NULL
parallel type 4 -> 0x82326608 -> 0x82326420
inner:   type 0 -> 0x82325E70
consumer matriciel virtuel -> 0x820EB200
```

La disparition d'`EndMode` et la continuation du travail visuel sont donc
deux faits distincts. La qualification complémentaire ferme toutefois ce
chemin comme contenu, pas comme transition : `0x820EB200` est le slot 7
virtuel d'une instance `CSwgRenderer` et transforme matrice/record SWG en un
record CPU de layout `0x70`, ensuite sérialisé et mis en file. Aucun accès au
mode manager n'apparaît dans ces corps. Le bord frontend n'est donc ni le
prédicat ActionScript supposé, ni un effet aval de ce renderer.

## Taxonomie structurelle utile

| Élément | Nature prouvée |
|---|---|
| `0x2E3EDA90`, `0x2E4035D0` | instances `swg::MovieController`, vtable RTTI globale `0x820304D8` |
| `[owner+0x10]` | pointeur d'attribut vers une instance `swg::MovieMemory`, vtable `0x8200654C` |
| `owner+0x28/+0x2C` | champs début/fin de table de frames |
| `owner+0xD8/+0xDC/+0xF8` | champs curseur interne/frame publiée/commande courante |
| `0x82323808` | initialiseur/constructeur d'instance owner |
| `0x82326B80` | helper d'initialisation du wrapper frame/storage |
| `0x82323BB8` | méthode d'update d'instance et dispatcher de commandes |
| `0x82322A80` | handler type 6 qui ajoute le payload à `MovieMemory` |
| `0x8264CE6C`, `0x8264CE54`, `0x8264D074` | tables globales read-only de dispatch |
| `0x82386408` | table globale read-only des commandes natives SWG |
| `0x82278F78` | fonction globale de décodage bitstream ; aucun rôle de prédicat |
| `0x82007D0C` | vtable globale read-only RTTI `CSwgRenderer` |
| `0x820EB200` | méthode virtuelle d'instance, slot 7 de `CSwgRenderer` |
| `0x82325E70` | callback de table globale ; appartenance à une classe non prouvée |

Ces noms décrivent uniquement des CFG et accès prouvés ; ils ne constituent
pas des symboles source Project Aces.

La taxonomie `CModeTaskTitleDemoOffline` et `CTaskModeManager` a aussi été
resserrée dans `analysis/demo/ac6-demo-structural-types-v1.json` : factory
globale, constructeur, méthodes d'instance/virtuelles, cellules globales,
sous-objets et champs y restent séparés. Une revue indépendante valide le
snapshot SHA-256
`e1e44df6c974a92f32315810aa47bb0ec74ca8bc90249859649008d65f0c9afa`.

## Frontières restantes

- `EndMode` : les douze descripteurs bruts `brandLogo` totalisent 2 351
  frames et onze éléments type 6, sans payload littéral `0xE04`. Il reste à
  joindre leurs offsets à la représentation heap décompactée ; l'absence
  brute ne réfute pas une relocation.
- Descripteur START : le même LR `0x82323860` prouve la republication par le
  chemin d'initialisation, sans distinguer nouvelle instance/adresse
  réutilisée ni changement de storage `B`, descriptor `D`, ou des deux.
- Frontend : identifier l'état persistant de `CModeTaskTitleDemoOffline` ou
  une transition indépendante qui choisit Loading/GameDemoOffline ou arme le
  manager ; le chemin renderer type 4 n'en est pas le producteur.
- La chaîne `menu_endMode -> listener Title -> état 2 / compteur ->
  manager+0x18` reste valide **si** `menu_endMode` est invoquée ; elle n'est
  pas une jonction naturelle START prouvée.

La trace proposée au cycle 1787 sur `M102/0x0B` est annulée. Aucun runtime
n'est justifié à ce point.

## Validation

- build codegen-OFF : PASS ;
- CTest codegen-OFF : 26/27, seul `ac6-demo-complexity` échoue comme dette
  connue ;
- build codegen-ON : PASS ;
- aucun OOM, compilateur tué ou échec de création de processus.

Le test de complexité ne sera pas résorbé avant le premier gameplay visible.
Les anciens échecs de build étaient des erreurs de déclarations de hooks de
trace, désormais absentes ; aucun lien causal avec un processus AC5 n'est
établi.

## Reçus autoritaires

- `artifacts/goal-playable/title-swg-post-start-static-correction-20260823/RESULT.md`
- `artifacts/goal-playable/title-frame-descriptor-static-20260823/RESULT.md`
- `artifacts/goal-playable/title-parallel-matrix-consumer-static-20260823/RESULT.md`
- `artifacts/goal-playable/title-start-route-static-20260823/RESULT.md`
- `artifacts/goal-playable/start-title-object-rtti-static-20260823/RESULT.md`
- `artifacts/goal-playable/frontend-mode-manager-canonical-ghidra-20260823/RESULT.md`
- `analysis/demo/ac6-demo-structural-types-v1.json`
- `artifacts/goal-playable/cycle-1788-static-correction-validation-20260823/RESULT.md`

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

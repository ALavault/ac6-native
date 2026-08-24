# Cycle 1792 — START Title accepté, route EndMode directe réfutée

Date : 2026-08-23  
Cible : démo PAL `Default.xex`  
SHA-256 XEX : `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`  
Projet Ghidra canonique : `ghidra-projects/ace-combat-6-demo`

## Verdict

`START_ACCEPTED / DIRECT_ENDMODE_ROUTE_REFUTED / SWG_ACC_MATERIALIZER_OPEN / FRONTEND_OPEN`.

Une unique exécution bornée prouve que le pulse START courant n'est pas perdu :
il atteint l'état logique guest, deux receivers VM par le slot virtuel `+0x70`,
puis exactement les cinq callbacks natifs du TitleUS. Dans la fenêtre qualifiée,
ce burst ne sélectionne ni `EndMode` ni `menu_endMode`, n'arme pas
`CTaskModeManager+0x18` et ne modifie pas l'état du vrai Title.

Cette fermeture ne vaut que pour l'occurrence et la fenêtre observées. Elle
n'exclut pas un événement de contenu guest distinct ou ultérieur et ne fournit
aucune preuve de prompt ou de frontend visible.

## Run unique borné

Le script vérifie le binaire codegen-ON SHA-256
`541b7824c78f1ad70dbadbc67ffc0ca0e7ffa25d1d9b4678985fef6b984f9dd8`, le
manifest codegen `465c279f529932165cd1d00416ff9b3d237ba3932a01874aea6cda6448994c76`
et les neuf fichiers du store. Il refuse tout environnement hérité
`AC6_DEMO_*`, injecte START au tick 3000 puis le relâche au tick 3001, observe
`2990:3041` et s'arrête à `max_ticks=3041` si `frontend` reste absent.

Configuration : `SDL_AUDIODRIVER=dummy`, Xvfb, backend `headless`, cgroup
`MemoryHigh=16G`, `MemoryMax=24G`, `TasksMax=128`, timeout 45 minutes. Aucun
`FORCE`, `INJECT`, `EXPERIMENTAL`, remapping, changement scheduler ou pixel
synthétique n'est employé. `probe.status=4` est l'issue normale `max_ticks` ;
le scope systemd termine `Result=success`, sans trap.

## Chaîne guest positive

Pendant le pulse, la globale de normalisation `0x827B37E0` atteint
transitoirement `0x400`, puis reçoit des écritures à zéro. Les globales
logiques atteignent :

```text
tick 3000  [0x82798480] current = 0x10
           [0x82798488] pressed = 0x10
tick 3001  [0x82798484] previous = 0x10
           [0x82798488] pressed revient à 0
```

Au tick 3001, deux appels passent par le thunk `0x820D32D0`, la vtable
`0x82006A9C`, le slot `+0x70`, puis la méthode virtuelle `0x820D3AC8`, sur
les receivers dynamiques `0x2E403CD4` et `0x2E403994`. Le même contexte SWG
`0x2E3EAA94` appelle ensuite exactement :

```text
row 0x82386658 -> 0x820EA598  GetCurrentLevel
row 0x82386648 -> 0x820EA550  GetCurrentMission
row 0x82386638 -> 0x820EA538  GetCurrentMode
row 0x82386478 -> 0x820E9838  SendMsgI(M102)
row 0x82386568 -> 0x820EA6C0  OnVoice2D enqueue
```

## Négatif borné et fermeture statique

`0x820EA4A8` n'apparaît jamais dans `[2990,3041)` ; son seul appel du run est
le `EndMode` Startup au tick 2365, avant l'entrée. Tous les snapshots de mode
2990..3040 sont identiques : manager `0x18980000`, Title courant
`0x2E3C0100`/vtable `0x820113E4`, état `1`, `manager+0x18=0`,
`Title+0x44=3` et `Title+0x70=0`. Aucun `MODE_SWITCH` ou `MODE_INNER` ne suit
le pulse.

La statique ferme ensuite les alternatives déjà nommées : census exhaustif
des writers `MovieController+D5`, reconstruction
`0x820D18C8 -> 0x82323808`, quatre formes de lancement SWG, plage
owner/frame et chaîne commune opcode `0x4D -> 0x820E8F90 -> descriptor ->
0x820EA4A8`. Aucun de ces chemins n'est joint à un producteur post-TitleUS
distinct qui atteigne `0x820EA4A8` ou le listener Title `0x8217C890`.

Le faux payload littéral `{type=6,raw=0xE04}` reste exclu de l'exhaustif
domaine outer-type-6 TitleUS et de la route naturelle object-102. Aucun shim,
appel de factory, écriture manager, réarmement ou changement produit n'est
justifié.

Le dispatch isolé au tick 3036 est également classé : `0x820D3280` est un
thunk global vers le slot `+0x6C` de la vtable RTTI
`swg::ASContext::String` `0x82006B44`; `0x820D7F80` est une méthode virtuelle
d'instance de conversion/copie/boxing String, appelée sous le helper générique
AS/VM `0x820DC008`. `0x820DC224` est son adresse de retour. Aucun de ces corps
ne rejoint EndMode, le listener, le manager ou un `MovieController`.

La jointure statique loader→descripteur ferme ensuite une couche de plus.
`0x82278F78` est un décompresseur bitstream global qui écrit les octets finaux
de l'image SWG, sans relocation, sélection de draw ni matérialisation de
record. `0x82322300` transmet le storage incorporé `B=parent+8` et le
descripteur `D` à la factory virtuelle `0x820D18C8`; l'initializer d'instance
`0x82323808` puis le helper global `0x82326B80` construisent seulement la vue
de range `owner+0x28/+0x2C/+0x30`.

Le premier item concret qualifié du `brandLogo` est statiquement type 4
(`D=0x1AA48`, frame table `0x718`, élément `0x704`), jamais type 2/6. Aucun
writer de cette chaîne ne fournit `record+0x0C=2`, le handle
`0x0E000059` ou les rows `0x82386628/0x82386698`. Cela déplace la frontière
vers le materializer SWG/ACC en amont ; le descripteur `brandLogo` reste une
donnée statique et n'est pas promu en owner Title runtime.

L'audit de ce materializer qualifie `swg::MovieMemory` et ferme un faux
producteur supplémentaire. `0x820D0DB8` initialise cet objet de 0x24 octets ;
les slots `0x820D0FD8`, `0x82358FD0`, `0x820D0FF8` et `0x820D1008` sont ses
méthodes virtuelles Add, Count, GetAt et Clear. Le constructeur ne touche que
ses propres tableaux. `0x823233B0/0x82323468`, précédemment présentés comme
opérations internes possibles de MovieController, sont reclassés par leur ABI
et leurs accès : ce sont des helpers globaux du manager ACC obtenu via
`0x82327108`, sans receiver B. Ils ne produisent donc ni `B+0x38`, ni count,
ni lien record #1→#2, ni draw index 2.

La cellule virtuelle `0x820064E8` qualifie toujours la factory
`0x820D18C8`, mais B et D sont des arguments fournis par son caller. Le seam
est ainsi réduit à ce caller naturel et au propriétaire exact de B/D ; le
littéral handle `0x0E000059` enregistré à `0x820C600C` reste une valeur de
ressource distincte d'une preuve de slot 2 dans l'owner rendu.

## Taxonomie sémantique

- `0x827B37E0` et `0x82798480/84/88` sont des données globales mutables,
  jamais des objets ou attributs du Title.
- `0x820D32D0` est un thunk de dispatch virtuel ; ce n'est pas une méthode
  métier autonome.
- `0x820D3AC8` est une méthode virtuelle d'instance VM/script. Les deux
  receivers runtime sont qualifiés, mais le nom de classe source reste ouvert.
- `0x82386658/48/38`, `0x82386478` et `0x82386568` sont des rows/descripteurs
  globaux ; leurs cibles sont des callbacks globaux SWG.
- `0x82278F78` est une fonction globale de décompression ; `0x82322300` est
  une méthode d'agrégat, `0x820D18C8` une factory virtuelle d'instance,
  `0x82323808` un initializer d'instance et `0x82326B80` un helper global.
- `B=parent+8`, `owner+0x20` et `owner+0x28/+0x2C/+0x30` sont respectivement
  un objet storage incorporé et des attributs d'instance ; `D` et les frames
  `{offset,count}` sont des records de données sérialisés.
- `swg::MovieMemory` est un objet RTTI-qualifié ; `0x820D0DB8` est son
  initializer et Add/Count/GetAt/Clear sont des méthodes virtuelles.
- `0x823233B0/0x82323468` sont des fonctions globales d'aide au manager ACC,
  pas des méthodes MovieController ni des writers du storage B.
- `0x2E3C0100` et `0x18980000` sont des instances runtime transitoires du vrai
  `CModeTaskTitleDemoOffline` et de `CTaskModeManager` ; leurs offsets qualifiés
  sont des attributs d'instance.

## Frontend et frontière suivante

Le report termine avec `frontend=false`, `mission=false`, `terminal=false`.
Ses 2933 notifications de présentation ne sont pas des frames frontend : avec
le backend headless, `ring.submissions=0`, `draw_count=24` et
`present_count=0`. Aucun prompt visuel n'est revendiqué.

La prochaine arête statique minimale est désormais :

```text
caller naturel de la cellule 0x820064E8 -> 0x820D18C8
  -> provenance et propriétaire des arguments B / D
  -> stores B+0x38, count, lien record #1 -> #2, record #2 draw=2
  -> slot 2 = 0x0E000059 et premier writer type-2 sur la route Title
  -> éventuel record type-2 sélectionnant row 0x82386628 ou 0x82386698
```

Une autre trace ne sera autorisée qu'après réduction de cette arête à un
prédicat causal nommé et un `done_when` plus précis. Le gate frontend conserve
la paire indissociable : état guest persistant et frame visible
post-transition. `supported=false` reste inchangé.

## Reçus et intégrité

- `artifacts/goal-playable/title-start-edge-runtime-final-20260823/RESULT.md`
  — SHA-256 `8d5eb40a483577bbe17e2fde7a4bb1cf279a6ccadbc8a8e1fc1caea0ea083271`.
- Replay — SHA-256
  `a121ad8d4fd78175c5c0e18b38ad027072f7411057b5e01592222e33b7531674`.
- Report — SHA-256
  `539251aabf29283280c4d127b58c46e15d1d8217d9902f442b7d4037ef047a20`.
- `artifacts/goal-playable/post-titleus-distinct-producer-static-20260823/RESULT.md`
  — SHA-256 `b134737c6b2c80ff57552d8653230b36aba2d46fbb90282db5ba5b9d7b43c407`.
- `artifacts/goal-playable/tick3036-thunk-static-20260823/RESULT.md`
  — SHA-256 `35d297b2fc78aceced46d91ff17538e21d82a17fad12dec6338a131098707a5a`.
- `artifacts/goal-playable/title-loader-descriptor-join-static-20260823/RESULT.md`
  — SHA-256 `283faf90b89da17af4e9b677c4ff468b010100283bb72f34a64cb0014afeb48c`.
- `artifacts/goal-playable/swg-acc-materializer-static-20260823/RESULT.md`
  — SHA-256 `6587ac00d343fbed1a1291f8726e633ef8d0243e57cde4f252ae9fef136875f8`.
- `analysis/demo/ac6-demo-structural-types-v1.json` porte les classifications
  et les bornes de preuve — SHA-256
  `9e5d906089e531a4e9cf3aad7c8d44cfdcc050f1c2ce1e616835b72e5a9e173a`.

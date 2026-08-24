# AC6 PAL démo — VM `TitleUS` et frontière `MovieMemory`, cycle 1789

Date : 23 août 2026  
Verdict réconcilié : **MOVIEMEMORY-WRITER-AND-TITLEUS-TYPE6-DOMAIN-CLOSED /
OBJECT102-FORWARD-ROUTE-CLOSED-THROUGH-FRAME-208 /
M102-RETURN-AND-ONVOICE-COMPLETION-EXCLUDED /
OBJECT102-E04-SELECTION-EXCLUDED /
MANAGER-INTERNAL-PREDICATE-NOT-TITLEUS-PRODUCER /
DISTINCT-GUEST-PRODUCER-OPEN / RENDERER-BASE8-CLOSED / FRONTEND-OPEN**.

## Qualification et portée probatoire

La cible unique est la démo PAL `Default.xex`, SHA-256
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.
Toute qualification de code guest provient du projet Ghidra canonique
`ghidra-projects/ace-combat-6-demo`, programme `/Default.xex`, langage
`PowerPC:BE:64:Xenon`. Le retail PAL, les prototypes Preview et le projet
historique `corrected` ne fournissent aucune preuve transférable sur les
classes `CModeTask*DemoOffline`.

Les fermetures causales ci-dessous sont statiques. Les runs bornés r2 et r3
sont utilisés uniquement pour leurs observables qualifiés. Ils ne remplacent
pas une identité d'objet absente et ne transforment pas une entrée de fonction
en preuve d'allocation, de publication ou de transition. Aucun shim, état
guest, record SWG, scheduler ou ressource n'a été forcé dans ces runs.

## Source titre et contexte d'exécution

La configuration courante choisit `DATA.TBL[178]`, `DATA00.PAC`, puis le fils
FHM `[0,0]` nommé `TitleUS`. Le record stocké, l'image mode 1 et le fils SWG
ont respectivement les SHA-256
`a045074e9b3dc0deae0deffe23df9deade10bf44801087f0e4d127ef7eb866f9`,
`a85f512b7c3c17ee98b74758327413e394e18a1e2931c4438df17f93a27732c0`
et `a01a6d592002796afe0c62bfcc8cdd8f0bcde73880cf4a0da15538b71e7921ad`.
La fenêtre heap `0x2DCB2000..0x2DCB3FFF` est égale aux 8192 octets
`TitleUS+0xDE0..+0x2DDF`. Cette égalité bornée ne prouve ni l'identité de
l'objet source runtime, ni l'égalité du buffer heap complet.

`0x82325160` est une méthode d'instance sur un contexte d'exécution
structurel : elle fetch un opcode big-endian, avance le PC et dispatche via la
table globale `0x8264CEA8`. `0x823251E0` reçoit un offset déjà choisi et écrit
exactement `PC = table_base + raw_offset`. Elle matérialise donc la destination
du statement, sans choisir le raw offset, la frame ou une branche.

## Writer `MovieMemory` et domaine naturel fermé

Dans le dispatch owner/frame qualifié, l'unique writer de la collection est :

```text
frame sérialisée
  -> record outer type 6
  -> table globale 0x8264CE6C, index 6
  -> callback global 0x82322A80
  -> MovieMemory::Add 0x820D0FD8(second_word)
  -> drain 0x82323BB8
  -> GetAt(raw)
  -> PC = table_base + raw
  -> stream borné jusqu'à opcode zéro
```

Le callback `0x82322A80` transmet le second mot `u32` sans relocation. Le
domaine exhaustif de `TitleUS` contient 40 payloads outer type 6 uniques et
exclut `+0xE04`. Pour l'objet 102, la séquence post-START naturelle est :

| Frame | Raw ajouté | Résultat borné |
|---:|---:|---|
| 149 | `+0x1210` | appels Title, guard, stop à `+0x1428` |
| 149 | `+0x142C` | `OnVoice2D`, discard, stop à `+0x1468` |
| 181 | `+0x146C` | aucun `0x4D`/`0x32`, stop à `+0x1718` |
| 198 | `+0x171C` | aucun `0x4D`/`0x32`, stop à `+0x1784` |
| 208 | `+0x1788` | opcode 2, puis zéro à `+0x178C` |

Après la frame 208, aucune frame ultérieure de l'objet 102 ne contient de
record outer type 6. Le silence natif aval ne dépend donc pas d'un appel
bloqué ou d'un retour natif mal émulé.

## Ordre exact de `M102` et du guard `SendMsgV`

L'item `+0x1210` contient six sites `0x4D` sérialisés, mais la route naturelle
n'envoie pas six appels natifs. L'ordre utile est :

```text
+0x127C  local 04 -> GetCurrentLevel
+0x12B8  local 05 -> GetCurrentMission
+0x12F4  local 06 -> GetCurrentMode
+0x134C  local 17 -> SendMsgI("M102")
+0x1350  opcode 1A -> état VM inline seulement
+0x13AC  local 0B -> miss/NULL, aucun dispatch natif
+0x13DC  calcul du booléen C
+0x13E0  négation !C
+0x13E4  branche +0x3C prise vers +0x1428 car C=false
+0x1420  local 18 -> SendMsgV, sérialisé mais sauté
+0x1428  zéro
```

Le guard est donc postérieur à M102 et choisit seulement de sauter
`SendMsgV`. Il ne choisit ni M102, déjà exécuté, ni l'item `+0x142C`, ni une
frame type 4, ni `EndMode`. Le retour `Integer(0)` de M102 ne modifie ni la
queue, ni `D5/D6/D8/DC/E0`, ni le PC de l'item suivant.

## `OnVoice2D` n'est pas une barrière Title

L'item indépendant `+0x142C` sélectionne la ligne globale immuable
`0x82386568`, puis le wrapper global `0x820EA6C0`. Celui-ci résout le nom,
appelle `CSwgSound::slot2`, puis `CNuSound::slot24/slot23`, alloue un handle et
publie un record voix guest. Le handle revient immédiatement, l'opcode
`0x17` le jette, puis le stream s'arrête.

Le worker audio asynchrone sonde ensuite le handle bas niveau et libère le
record terminé. Aucun champ callback ou dispatch vers Title,
`MovieController`, `EndMode` ou `CTaskModeManager` n'existe sur cette chaîne.
Le backend XMA reste une frontière d'audibilité séparée ; il n'est pas une
condition de continuation de `TitleUS`.

## Tail `+0x1788` et limite de réarmement

`TitleUS+0x1788` vaut exactement `{opcode 2, zéro}`. La cellule opcode 2
`0x8264CEB0` pointe vers le callback global `0x82324248`, qui atteint
`0x82322CD8/0x82322CE4` et efface le byte terminal
`MovieController+0xD5`. `0x82323BB8` lit `D5` pour décider l'incrément de
`D8`, mais n'écrit jamais `D5` ; il n'existe donc aucun réarmement autonome
dans l'update.

Un progrès ultérieur reste possible dans le contrat générique du moteur,
mais exige un dispatch guest distinct : callback de navigation, item lancé
indépendamment, opcode 1, opcode `0x34` sélecteur 1 ou 3, ou construction d'un
autre controller. Aucune de ces arêtes n'est produite par le tail courant ni
par une frame type 6 ultérieure de l'objet 102.

## `EndMode`, listener Title et armement manager

`TitleUS+0xE04` est un statement de bytecode de 24 octets, SHA-256
`03642527b4472b50740ed2a4644d4cd3349954254f820d0766dc48fe8f68fe0b`.
Il n'est ni un record de queue ni une frontière d'instruction naturelle quand
le stream commence à `+0xD0C`, où ses octets sont consommés comme payload
d'un opcode `0x2E`. Il ne devient un statement extérieur que si un launcher
initialise explicitement le PC à `+0xE04`.

Le XEX et les 861 payloads logiques ne contiennent aucun record littéral
big-endian `{type=6,payload=0xE04}`, et le domaine exhaustif des 40 payloads
type 6 de `TitleUS` l'exclut aussi. Cette fermeture réfute la sélection
naturelle par l'objet 102 ; elle ne réfute pas abstraitement un événement
guest distinct, un calcul d'offset ou un launcher indépendant.

Les quatre formes de launcher statiquement qualifiées convergent toutes vers
`0x82325160`, l'opcode `0x4D`, le marshaller virtuel `0x820E8F90`, puis une
ligne de descripteur. Pour atteindre le listener Title, la ligne doit être
`0x82386628` (`EndMode`) ou `0x82386698` (`menu_endMode`), toutes deux ciblant
le broadcaster global `0x820EA4A8`. Aucun item naturel post-START ci-dessus ne
sélectionne ces lignes.

L'appel naturel `0x820EA4A8(arg=2)` déjà observé sur le chemin attract neutre
est seulement un contrôle positif de l'existence d'une route sérialisée
valide. Son launcher et son offset initial restent non joints, et cette route
ne peut pas être transférée à la frame post-START.

La chaîne aval conditionnelle est fermée :

```text
0x820EA4A8
  -> Title+0x68, CSwgListener::slot+0x54 0x8217C890
  -> Title::slot+0x48 0x8218AB98
  -> Title+0x44 = 3 ; Title+0x0C = 2
  -> Title update 0x8218A7A8, compteur expiré
  -> Title update appelle Title::slot+0x4C 0x8218AA30
  -> Title update écrit manager+0x18 = 1 à 0x8218A800
  -> manager update 0x821929A8
  -> transition 0x82190B18
```

Cette chaîne ne prouve pas son déclencheur START. Le store interne manager
`0x82192CC8` n'est pas ce producteur : ses deux prédécesseurs exigent
`current_mode+0x48 != 0`, tandis que le vrai `CModeTaskTitleDemoOffline` est
construit avec `+0x48=0`, `+0x49=1`, `+0x4A=1`. Les chemins gouvernés par
`manager+0x5D/+0x5F` sont donc annulés ou bloqués pour ce Title. De même,
`0x82170F58` est l'update de `CModeTaskDemoBase` et de quatre dérivées, jamais
celui du vrai Title ; l'utiliser pour expliquer sa première transition serait
circulaire.

Le census canonique des stores directs résolus depuis la cellule
`0x827435F8` trouve 32 méthodes virtuelles d'instance qui peuvent écrire
`manager+0x18`. Seul le couple `0x8218A7A8/0x8218A800` appartient à la vtable
du vrai Title, et il présuppose déjà le callback et l'état 2. Les autres stores
appartiennent à des tâches distinctes, dont `CModeTaskDemoBase` et
`CModeTaskMissionTitle`. Le census ne prétend pas réfuter un alias arbitraire
transmis entre fonctions ; cette limite d'alias ne rouvre pas le domaine
naturel `TitleUS`, fermé indépendamment.

## Frame type 4 et renderer

La frame post-START type 4 appartient à un canal parallèle de
contenu/transformation/rendu :

```text
outer type 4 -> 0x823239F0 -> 0x82326608 -> 0x82326420
             -> sous-record type 0 -> 0x82325E70
             -> CSwgRenderer::slot7 0x820EB200
```

Elle n'appelle ni `MovieMemory::Add`, ni la VM d'invocation, ni `EndMode`, ni
le mode manager. Elle ne doit pas être substituée à la séquence outer type 6.

La divergence host `{base=8,count=4}` rencontrée par r2 au tick 3111 est
séparée. Le run r3 accepte le profil borné par un snapshot de 624 octets,
consomme les records vertex 8..11, atteint 3407 ticks sans nouveau trap et se
termine normalement sur `max_ticks`. Cette validation ferme le profil renderer
base 8 ; elle ne prouve aucune transition guest.

## Observations r3 et limites d'identité `MovieController`

Au terme de r3, `frontend=false`, `mission=false` et `terminal=false`. Le
traceur contient 136 entrées de la méthode d'instance
`CTaskModeManager::update` `0x821929A8`; le receiver manager et le pointeur de
tâche courante restent inchangés. Il contient aussi :

- une entrée de l'initialiseur `0x82323808` au tick 3105, receiver
  `0x2E3CED10`, parent `r6=0x2E3CDD10` non nul ;
- un record outer type 6 au tick 3282, owner `0x2E3CDD10`, raw `0x3F8`, sans
  ancre `MovieMemory` appariée ;
- un verdict final `INCONCLUSIVE/anchor_missing` au tick 3406.

`0x82323808` est statiquement un initialiseur d'instance
`swg::MovieController`, mais son entrée runtime seule ne prouve ni allocation
neuve, ni identité `TitleUS`, ni publication à l'agrégat du vrai Title. Le
parent non nul exclut le callsite racine qualifié, qui exige `r6=0`; un clip
enfant reste compatible. Le raw `0x3F8` ne peut donc pas être attribué à la
queue naturelle de l'objet 102 sans joindre le callsite, les descripteurs
`A/B/D/s`, le résultat factory et, pour la racine, le store `A+0xF4`.

Aucun `MovieMemory::Add` apparié, aucune ancre `0x1210 -> 0x142C`, aucune
exécution `+0x1788`, aucun store terminal, armement `manager+0x18` ou
remplacement de tâche n'a été observé par r3. L'absence d'ancre interdit de
convertir cette non-observation en réfutation dynamique de la séquence
statique.

## Taxonomie structurelle explicite

| Élément | Catégorie qualifiée | Limite ou rôle |
|---|---|---|
| `TitleUS`, offsets, frames, records type 4/6 | données sérialisées | jamais objets C++ ni fonctions |
| buffer `0x2DCB1220...` | buffer heap runtime | pas une globale XEX ; identité complète disque/heap non prouvée |
| `CModeTaskTitleDemoOffline` | objet/instance démo | RTTI et vtables propres à la démo PAL |
| `Title+0x1C` | sous-objet incorporé `CSwgManager` | distinct du listener `Title+0x68` et du mode manager |
| agrégat SWG `+0xF4` | champ pointeur | pointe vers un `swg::MovieController*` lorsqu'il est publié |
| `MovieController` / `MovieMemory` | objets runtime | controller, puis collection d'offsets `u32` à `+0x10` |
| `MovieController+D5/D6/D8/DC/E0/F8` | champs d'instance | contrôle timeline, latch seek, frame demandée, frame effective, dernière frame drainée, curseur record |
| `exec+C/10/14/18` | champs d'instance du contexte | handler, table base, PC, controller associé |
| `0x82323BB8` | méthode d'instance | update/sélection/drain du `MovieController` |
| `0x82323808` | initialiseur/constructeur-like d'instance | l'identité d'une invocation runtime exige la jointure factory/callsite |
| `0x823251E0`, `0x82325160` | méthodes d'instance structurelles | start-at-offset et dispatcher ; nom de classe source non promu |
| `0x82322A80` | callback global de table | handler outer type 6 ; appartenance C++ non prouvée |
| `0x82324230..0x823242D8`, `0x82324930` | callbacks globaux de table | handlers navigation/opcode ; pas méthodes d'instance |
| `0x8264CF78` | cellule globale constante | index `0x34` de `0x8264CEA8`, contient `0x82324930` ; `0x82324930` n'est pas la cellule |
| `0x8264CEA8`, `0x8264CE6C` | tables globales de pointeurs | dispatch opcode et dispatch outer |
| `0x82386568`, `0x82386628`, `0x82386698` | lignes globales de descripteur | données immuables, pas callbacks ni objets |
| `0x820EA6C0`, `0x820EA4A8` | fonctions/callbacks globaux | wrapper `OnVoice2D` et broadcaster EndMode/menu_endMode |
| `0x826DFC44` | cellule globale mutable | contient un `CSwgSound*`, pas un champ Title |
| `0x826DF800..0x826DF83F` | table globale mutable | 16 pointeurs `CSwgListener*`, pas 16 objets tâche |
| `Title+0x68` / `0x8217C890` | sous-objet / méthode virtuelle d'instance | listener secondaire du vrai Title |
| `0x827435F8` | cellule globale mutable | contient un `CTaskModeManager*`, pas l'objet manager |
| `manager+0x18` | champ d'instance `u32` | flag d'armement, distinct du bit START global |
| `0x821929A8`, `0x82190B18` | méthodes d'instance | update et transition du manager |
| `0x82798488` | cellule globale mutable | bitset pressed transitoire ; `0x10` est START logique |
| `0x820E8F90` | méthode virtuelle d'instance | marshaller native du handler SWG |
| `0x820E9130` | adresse de reprise | `cmplwi` dans `0x820E8F90`, ni fonction, ni callsite, ni launcher |

## Frontière restante

Le domaine naturel `TitleUS` post-START, le writer `MovieMemory`, le guard
M102/SendMsgV, la complétion `OnVoice2D`, le tail `+0x1788`, le prédicat
interne manager et le profil renderer base 8 sont fermés dans leurs portées
respectives.

La frontière fonctionnelle commence désormais hors de ce domaine naturel :

```text
producteur guest distinct non encore joint
  -> soit navigation/item indépendant qui réarme D5 ou choisit un autre stream
  -> soit invocation légitime du listener Title et passage à l'état 2
  -> soit construction/insertion préalable d'une autre tâche
  -> manager+0x18 = 1
  -> CTaskModeManager::0x82190B18
```

La jointure d'identité `0x820D18C8 -> 0x82323808 -> résultat -> A+0xF4` reste
une ambiguïté d'objet séparée. Même fermée, elle ne constituerait pas à elle
seule une preuve de transition frontend. Aucun shim, remap, record synthétique
ou modification d'état guest n'est justifié par les reçus présents.

## Reçus autoritaires réconciliés

- `artifacts/goal-playable/titleus-post-start-vm-static-20260823/RESULT.md` ;
- `artifacts/goal-playable/title-listener-alternate-launchers-static-20260823/RESULT.md`
  et `RESULT-followup.md` ;
- `artifacts/goal-playable/title-onvoice2d-completion-static-20260823/RESULT.md` ;
- `artifacts/goal-playable/title-guest-transition-state-static-20260823/RESULT.md` ;
- `artifacts/goal-playable/title-manager-arm-static-20260823/RESULT.md` ;
- `artifacts/goal-playable/manager-arm-predicate-static-20260823/RESULT.md` ;
- `artifacts/goal-playable/title-swg-relocation-map-static-20260823/RESULT.md` ;
- `artifacts/goal-playable/title-terminal-trace-runtime-20260823-r2/RESULT.md` ;
- `artifacts/goal-playable/title-terminal-trace-runtime-20260823-r3/RESULT.md` ;
- `analysis/demo/ac6-demo-structural-types-v1.json`.

# AC6 démo — orchestration, services de mission et débriefing

Date : 2026-08-17  
Portée : analyse statique Xbox 360, sans exécution Xenia Edge  
Branche cible : `infos`

Ce rapport consolide les travaux restés hors du dépôt après la fermeture du chemin de dégâts. Les tables de preuve et les extracteurs reproductibles sont livrés dans `analysis/ac6-demo-orchestration-service-debriefing-evidence-20260817.zip`.

## Sommaire

1. Dégâts et difficulté
2. Grammaire Set / Act / Order
3. Continuité AC5 → AC6
4. Registre des services de mission
5. Pont MissionManager → radio / HUD / HSM
6. Fermeture des frontières résiduelles
7. Corrections et rétractations
8. Débriefing et replay tactique
9. Prochain sous-système : classement


---

## 1. Dégâts et difficulté

### AC6 démo — formule de dégâts et difficulté

Date : 2026-08-17  
Méthode : analyse statique PPC, aucune exécution Xenia Edge  
Convention : les adresses `demo:` appartiennent au XEX de la démo

#### Verdict

L'hypothèse proposée est fonctionnellement proche, mais l'implémentation sépare
les trois facteurs :

```cpp
nominal_damage =
    float(WeaponBin.base_damage_u8)
  + state_modifier[map(object.state_0x108)][10];

effective_loss =
    nominal_damage
  * target.DurableBin[event_code];

target.max_durability =
    target.base_durability
  * difficulty_survivability[difficulty];
```

Le jeu ne multiplie donc pas directement le dommage par la difficulté. Il
multiplie la réserve maximale de durabilité des classes éligibles. Pour le
joueur, c'est équivalent à modifier la pression des dégâts en sens inverse.

#### 1. Facteurs exacts

`demo:0x8224F6C0` récupère un réglage prenant les valeurs 1 à 5. La table
d'affichage associée contient exactement :

```text
1 EASY
2 NORMAL
3 HARD
4 EXPERT
5 ACE
```

La fonction applique les multiplicateurs suivants à la durabilité maximale :

| Difficulté | Facteur de durabilité | Pression de dégâts équivalente |
|---|---:|---:|
| Easy | 1,5 | ×0,6667 |
| Normal | 1,0 | ×1 |
| Hard | 0,5 | ×2 |
| Expert | 0,33333334 | ×3 |
| Ace | 0,25 | ×4 |

Le « facteur de difficulté croissant » existe donc bien si on le définit comme
la pression effective du dommage. Dans le code, c'est son inverse qui multiplie
les points de structure.

#### 2. Exemple des chasseurs de Mission 01

| Appareil | Base | Easy | Normal | Hard | Expert | Ace |
|---|---:|---:|---:|---:|---:|---:|
| F/A-18F | 40 | 60 | 40 | 20 | 13,333 | 10 |
| Rafale M | 40 | 60 | 40 | 20 | 13,333 | 10 |
| Su-33 Strigon | 48 | 72 | 48 | 24 | 16 | 12 |

Strigon conserve donc exactement son avantage structurel de 20 % à toutes les
difficultés. Le réglage global ne remplace pas la différence d'archétype.

#### 3. `DurableBin` n'est pas la réserve de vie

Deux notions doivent rester distinctes :

- `base_durability` et `current_durability` sont la réserve de structure ;
- `DurableBin[event_code]` est un coefficient de sensibilité au type
  d'événement.

Le nombre approximatif d'impacts nécessaires est donc :

```text
base_durability × difficulty_survivability
────────────────────────────────────────────
(base_damage + state_modifier) × vulnerability
```

Appeler les deux notions « durabilité » produirait une formule ambiguë, puis
une implémentation incorrecte avec une remarquable efficacité.

#### 4. Le modificateur d'arme n'est pas la difficulté globale

`demo:0x822B6FE0` utilise la valeur signée `object+0x108` pour choisir une ligne
de table, puis la colonne demandée par l'appelant.

Le même mécanisme modifie :

- la dispersion ;
- plusieurs portées ;
- les vitesses ;
- le terme de dommage ;
- d'autres paramètres d'arme.

`object+0x108` est réinitialisé ou modifié lors des changements d'Act et
d'état de l'unité. Ce mécanisme est donc un contexte d'unité ou d'ordre, pas
le sélecteur global Easy/Normal/Hard/Expert/Ace.

#### 5. Limite

La multiplication par difficulté est précédée d'un prédicat virtuel. Le résultat
est donc prouvé pour les catégories d'objet qui optent dans ce chemin, pas pour
chaque objet du moteur sans distinction. Un autre mode global peut également
forcer le retour à la valeur de repli non redimensionnée.

---

## 2. Grammaire Set / Act / Order

### AC6 démo — fermeture statique de SetBin, ActBin et OrderBin

Date : 2026-08-17  
Corpus analysé : racine gameplay de la mission de la démo  
Méthode : parse structurel et analyse des consommateurs PPC

#### Résumé

La grammaire de mission est maintenant suffisamment fermée pour être
réimplémentée sans modèle de script inventé :

```text
SetBin
  └── ActBin[]
        └── OrderBin[]
```

- `SetBin` contient les scripts alternatifs d'un objet ;
- `ActBin` est un script ordonné ;
- `OrderBin` est une instruction typée ;
- l'objet runtime conserve l'Act courant à `+0x10C` ;
- l'ordre courant est conservé à `+0x110` ;
- la valeur `-2` signifie que la séquence est épuisée.

La racine gameplay de la démo contient :

```text
213 groupes
213 SetBin
471 ActBin
2 875 OrderBin
414 ObjBin gameplay
```

#### 1. Layout runtime

### SetBin

```text
+0x00  pointeur vers le record brut
+0x04  tableau de wrappers ActBin
```

Le nombre d'Acts est le premier byte du record brut. Chaque wrapper Act occupe
8 octets.

### ActBin

```text
+0x00  pointeur vers le record brut
+0x04  tableau de wrappers OrderBin
```

Le nombre d'ordres est le premier byte du record brut. Chaque wrapper Order
occupe exactement 44 octets.

### OrderBin

| Offset | Tag | Sous-structure |
|---:|---:|---|
| `+0x04` | 0 | base / condition de départ |
| `+0x08` | 1 | Disappear |
| `+0x0C` | 2 | mouvement spatial |
| `+0x10` | 3 | Stop |
| `+0x14` | 4 | Lead |
| `+0x18` | 5 | Jump |
| `+0x1C` | 6 | Flag |
| `+0x20` | 7 | marqueur sans payload |
| `+0x24` | 8 | Property |
| `+0x28` | 9 | broadcast d'événement |

#### 2. Curseur d'exécution

Les résolveurs principaux sont :

```text
demo:0x82242288  Act courant
demo:0x822422B0  nombre d'ordres de l'Act
demo:0x82242378  Order courant
demo:0x822446F0  avance ou sélectionne l'Order
demo:0x82244F18  commence un Act
demo:0x82244B90  exécute l'Order courant
```

`demo:0x822446F0` accepte :

- `-1` pour avancer ;
- un index explicite pour sauter ;
- `-2` comme état terminal.

Lors de l'entrée sur un ordre de type 2 ou 3, il appelle immédiatement leur
initialiseur.

#### 3. Instructions

### Type 2 : mouvement spatial

```text
init    demo:0x82243A18
update  demo:0x82243BA0
```

Le payload contient des positions, orientations, vitesses/temps, conditions,
flags, mode et indices de cible. Il ne s'agit pas d'un simple waypoint plat.

Les modes observés dans la mission sont :

```text
0: 693
1: 44
2: 1
3: 47
4: 12
7: 87
```

Le mode 7 est la descente balistique utilisée par les objets `c_arbn`.

### Type 3 : Stop

```text
init    demo:0x82242778
update  demo:0x822429D0
```

Le record fournit au minimum deux flottants et des conditions. Selon son mode,
il attend un temps, un état ou un prédicat.

### Type 5 : Jump

Le payload comporte :

```text
u16 predicate_or_flag
s16 expected_value
u8  mode
u8  threshold_or_repeat
s8  relative_bookkeeping
u8  target_order_index
```

Les chemins observés couvrent :

- boucle bornée par compteur ;
- test d'un prédicat de mission ;
- saut inconditionnel.

### Type 6 : Flag

Le record contient :

```text
u16 service_id
s16 value
u8  mode
```

Dans la mission :

- mode 0 effectue un `set` ;
- mode 1 effectue un `add`.

Le service commun est `demo:0x82210190`.

### Type 8 : Property

Le record contient un identifiant 16 bits. `demo:0x82240808` publie la nouvelle
propriété sur l'objet et vers ses consommateurs.

### Type 7 : marqueur d'Act de repli

Le type 7 n'a pas de payload métier. Un Act dont le premier ordre vaut 7 est
recherché par `demo:0x82245230`, sélectionné, puis le marqueur est immédiatement
sauté.

Le nom interne reste ouvert, mais son rôle structurel est fermé : il marque une
route alternative déclenchée par le moteur, typiquement un repli, une
notification ou une fin de cycle.

### Type 9 : événement enfant

Le dispatcher diffuse un événement `1026` et un flottant à chacun des enfants.
Ce type est supporté par le code mais absent de la racine gameplay de Mission 01.

#### 4. Scripts discriminants de Mission 01

### C-17 et largage

Le script représentatif du groupe 9 est :

```text
Base
Motion
Flag
Flag
Motion
Property
Motion
Flag
Stop
Jump
```

puis un Act secondaire :

```text
Disappear
Jump
```

Les groupes `c_arbn` 62–72 combinent le mode spatial 7, les flags de largage,
un Act Disappear et, pour plusieurs groupes, un Act marqué par le type 7.

Cette structure explique une multiplicité runtime différente du simple nombre
de templates : le largage est piloté par l'orchestrateur et ses flags.

### Vagues conditionnelles

Les groupes 118 et 119, F/A-18F et Rafale M, partagent le même script de vol.
Leur Act secondaire marqué type 7 publie toutefois des opérations différentes :

```text
groupe 118 : service 164, +1
groupe 119 : service 279, set 2
```

### Strigon

Les groupes 165–172 possèdent deux Acts :

- un script tactique long avec propriétés 128, 0 et 129 ;
- un Act de disparition.

Ils publient des flags autour des services 219–223 et disposent de séquences
spatiales plus complexes que les renforts ordinaires.

### Nimbus

Les groupes candidats 200–208 ont la même forme :

```text
Act 0:
  Base
  Property 3
  Stop(10, 30000 ou 60000)
  Flag service 243..251, value 0
  Jump terminal

Act 1:
  Disappear
  Jump terminal
```

C'est une signature de contrôleurs scriptés d'une salve ou d'un événement
longue portée, et non de projectiles autonomes.

#### 5. Frontière résiduelle

Le langage et son exécuteur sont fermés. Ce qui reste ouvert est le dictionnaire
métier des services et propriétés :

```text
service 164, 219..223, 243..251, 279
property 0, 1, 3, 8, 128, 129
```

La prochaine passe doit relier ces identifiants à :

- activation de groupe ;
- objectif ;
- radio/HUD ;
- Nimbus ;
- retraite ;
- Return Line.

---

## 3. Continuité AC5 → AC6

### AC5 ↔ AC6 — continuité de la grammaire de mission

#### Verdict

`SetBin`, `ActBin` et `OrderBin` ne sont pas une invention propre à AC6. AC5
possède la même hiérarchie et les mêmes primitives fondamentales.

La comparaison montre une continuité de moteur, pas seulement une réutilisation
de noms.

#### 1. Classes présentes dans AC5

Le binaire PAL d'AC5 expose notamment :

```text
CSetBin
CActBin
COrderBin
COrderDisappearBin
COrderStopBin
COrderLeadBin
COrderJumpBin
COrderFlagBin
```

Le dispatcher runtime AC5 traite les types 0 à 6. Le type 2 ne possède pas de
nom de classe récupéré, mais son comportement spatial est désormais largement
reconstruit.

#### 2. Correspondance des tags

| Tag | AC5 | AC6 |
|---:|---|---|
| 0 | base/inline | base/action-start |
| 1 | Disappear | Disappear |
| 2 | mouvement spatial | mouvement spatial |
| 3 | Stop | Stop |
| 4 | Lead | Lead |
| 5 | Jump | Jump |
| 6 | Flag | Flag |
| 7 | absent du dispatcher connu | marqueur d'Act alternatif |
| 8 | absent du dispatcher connu | Property |
| 9 | absent du dispatcher connu | broadcast événement |

AC6 étend donc le langage, mais n'en change pas les fondations.

#### 3. Preuve comportementale décisive : mode 7

Dans AC5, le type 2 mode 7 est qualifié sur *White Bird (Part I)* :

- vitesse verticale initiale négative ;
- accélération sérialisée appliquée chaque tick ;
- fin sur flag ou contact terrain ;
- emploi par les chars aéroportés et parachutes.

Dans AC6, les groupes `c_arbn` utilisent également le type 2 mode 7 dans la
phase de largage de Gracemeria.

Cette concordance ferme le rôle de la primitive AC6 beaucoup plus fortement
qu'une simple comparaison de chaînes.

#### 4. Différences de payload

Le type 2 AC5 contient déjà :

```text
12 flottants
condition flag/value
champ signé
flags
mode
indices groupe/objet
bytes de variante
```

AC6 conserve cette organisation générale et l'étend ou décale légèrement. Les
noms de champs ne doivent toutefois pas être copiés mécaniquement entre les
builds : la compatibilité prouve la famille sémantique, pas l'identité ABI de
chaque offset.

#### Conséquence

La réimplémentation native devrait employer un interpréteur commun conceptuel :

```text
Set → Act → Order → runtime services
```

avec des codecs versionnés AC5 et AC6. Dupliquer deux moteurs de script
entièrement séparés serait possible, comme beaucoup de mauvaises idées le sont,
mais ne refléterait pas l'architecture observée.

---

## 4. Registre des services de mission

### AC6 démo — fermeture statique du registre de services et de l'orchestration de mission

Date : 2026-08-17  
Portée : analyse statique PPC, données de mission et consommateurs

#### Résumé

La chaîne suivante est désormais fermée jusqu'au niveau des sorties de mission :

```text
OrderFlag / événements monde
        ↓
registre de 327 services
        ↓
361 déclencheurs, 569 conditions
        ├──→ sélection d'un Act dans SetBin
        ├──→ mutation d'un objectif/statistique
        ├──→ sélection d'une ligne radio
        └──→ progression de phase SubMis

OrderProperty
        ↓
état comportemental de l'objet et de ses enfants
        ↓
notification manager
```

Le registre n'est pas une table de booléens. Chaque entrée possède valeur,
timestamp et état des déclencheurs.

#### 1. Layout d'un service runtime

Les services sont stockés à partir de `owner+0x58`, avec un stride de 20 octets :

| Offset | Rôle |
|---:|---|
| `+0x00` | valeur signée courante |
| `+0x04` | timestamp du premier passage à `1` |
| `+0x08` | masque des triggers déjà consommés |
| `+0x0C` | masque des triggers autorisés |
| `+0x10` | nombre/étendue de triggers |

Le temps courant de mission se trouve à `owner+0x60`.

La mutation canonique est `demo:0x82210190`. Le record accepte :

```text
mode 0 : set
mode 1 : add
mode 2 : random 1..value
mode 3 : somme de deux services
```

#### 2. Déclencheurs automatiques

`demo:0x82214CF8` parcourt les services et leurs triggers. Chaque trigger a :

- un record d'effet ;
- zéro ou plusieurs records de condition ;
- un bit d'activation ;
- un bit « déjà consommé ».

Mission 01 contient :

```text
327 services
361 triggers/effects
569 conditions
```

Les familles de conditions employées sont :

| Type | Nombre | Rôle minimal |
|---:|---:|---|
| 1 | 74 | temps, délai ou timeline |
| 2 | 307 | comparaison de valeur de service |
| 4 | 96 | état/quantité/durabilité d'un groupe ou objet |
| 5 | 74 | position, distance ou appartenance à une région |
| 9 | 18 | statistique d'objet |

Les effets sont :

```text
320 set
39 add
1 random
1 modification de statistique de mission
```

Le dernier chemin appelle `demo:0x8220F6E8` et modifie une statistique bornée du
MissionManager.

#### 3. Conditions de démarrage des Acts

Le premier `OrderDisappearBin` de certains Acts alternatifs contient également
le descripteur de condition de démarrage.

`demo:0x822455D0` évalue cette condition puis appelle `demo:0x82244F18`.

Le payload contient au minimum :

```text
+0x00  seuil temporel
+0x04  second paramètre flottant
+0x08  valeur attendue
+0x0A  service_id
+0x0C  mode de condition
+0x0D  sélecteur de timeline
+0x0E  notification post-démarrage
```

Répartition :

```text
176 Acts conditionnés par service
29 Acts conditionnés par timeline/progression
```

Le tag 7 reste un marqueur séparé pour les Acts alternatifs sélectionnés par
`demo:0x82245230`.

#### 4. OrderProperty

`demo:0x82240808` écrit :

```text
object+0x70 = property_id
```

puis propage cette propriété aux enfants et notifie un manager global lorsqu'elle
change.

Les propriétés les plus discriminantes sont :

```text
1      transport/largage C-17 et c_arbn
3      contrôleurs scriptés longue portée/Nimbus
8      vagues de chasseurs
128    phase tactique Strigon
129    transition tactique Strigon
0      état par défaut/base
```

Ces noms restent des associations comportementales, pas des noms d'enum source.

#### 5. Strigon

Les services `219..223` sélectionnent les Acts secondaires des éléments Strigon :

```text
219 → groupe 165
220 → groupe 166
221 → groupe 167
222 → groupe 168
223 → groupe 169
```

Ils sont également écrits par les scripts tactiques des Su-33.

La chaîne exacte est :

```text
OrderFlag Strigon
→ service 219..223
→ condition de démarrage
→ Act secondaire de l'élément
```

#### 6. Nimbus

Les services `243..251` commandent les paires :

```text
243 → groupes 191 et 200
244 → groupes 192 et 201
...
251 → groupes 199 et 208
```

Le service `293` est produit par la conjonction :

```text
service 258 == 1
ET
service 259 == 1
```

Il conditionne également des Acts pour les groupes `182..199` et le contrôleur
`209`. Lorsque `293` passe à 1, les services `243..251` sont remis à zéro par
les triggers correspondants.

`294` fournit une seconde barrière pour `244..251`, mais son producteur n'est pas
présent dans les triggers ou OrderFlags locaux. Il est probablement publié par
un manager externe ou par le cycle de vie d'une ligne radio.

#### 7. Service 279 et vague Rafale

Le service `279` possède les chemins :

```text
après 120 secondes et 279 == 0 → 279 = 1
si service 162 == 2           → 279 = 2
OrderFlag du groupe 119       → 279 = 2
```

`RadioTblBin` distingue précisément les valeurs 1 et 2 de ce service pour
l'événement `575`.

La chaîne est donc :

```text
timeline / mission state
→ service 279
→ vague Rafale / état de vague
→ variante radio
```

Le service `164`, alimenté par le groupe F/A-18F `118`, ressemble davantage à
un compteur de vague ou de résultat, mais son consommateur métier exact reste
moins discriminant.

#### 8. RadioTblBin

Le `RadioTblBin` de la mission contient 53 records de 56 octets.

Chaque record peut conditionner un événement radio par un à trois services :

```text
event 575 : service 279 == 1 ou 2
event 243 : service 282 == 1
event 251 : service 283 == 1
event 252 : service 286 == 1 ou 2
event 259 : service 287 == 1 ou 2
event 229 : service 291 == 1 ou 2
event 265 : service 293 == 1
event 258 : service 301 == 1 ou 2
```

Les événements `199..205` sont reliés aux services `315..322`. Les quatre
variantes terminales utilisent les valeurs 1..4 du service `325`.

La moitié voix/audio ayant déjà été fermée, le pipeline complet devient :

```text
services de mission
→ condition RadioTblBin
→ événement radio logique
→ identifiant de voix
→ banque ENG/JPN
→ RIFF XMA1
```

#### 9. SubMisTblBin et phases

Le `SubMisTblBin` contient trois soumissions et cinq descripteurs :

```text
soumission 0 : type 1, puis type 0
soumission 1 : type 1, puis type 0
soumission 2 : type 0
```

Les fonctions principales sont :

```text
demo:0x822100A8  avance curseur SubMis
demo:0x82211B88  exécute le descripteur courant
```

Champs runtime :

```text
owner+0x10   index de soumission
owner+0x14   index de descripteur
owner+0x28   progression/temps
owner+0x2A4  descripteur courant
```

La troisième phase utilise un tuple spatial distinct, les bornes `-50000` et
`-37000`, une limite de 300 secondes, les flags `0x251` et le sélecteur
`0x00020001`. Elle est fortement compatible avec la phase de retraite et la
Return Line ouest.

#### 10. Pont sorties

Le MissionManager per-frame `demo:0x822158B0` enchaîne :

- évaluation des services ;
- progression SubMis/HSM ;
- application de propriétés ;
- publication aux managers de présentation.

Le pont objectif est fermé comme mécanisme :

```text
condition
→ effet statistique
→ demo:0x8220F6E8
→ MissionManager
```

Le chemin vers le HUD est borné jusqu'à un dispatch virtuel de présentation,
mais le widget/méthode HUD final reste à nommer.

#### Verdict

| Front | État |
|---|---|
| OrderFlag → registre | fermé A |
| triggers automatiques | fermés A |
| service → Act | fermé A |
| OrderProperty → objet/enfants/manager | fermé A- |
| services → radio | fermé A- |
| services/statistiques → objectifs | fermé B+ |
| SubMis → HSM | fermé A- |
| phase terminale → Return Line | candidat fort A-/B+ |
| manager → widget HUD exact | borné, dispatch ouvert |

---

## 5. Pont MissionManager → radio / HUD / HSM

### AC6 — pont objectif / radio / HUD / HSM

#### Radio

```text
OrderFlag ou trigger
→ service_id / valeur
→ RadioTblBin
→ événement radio
→ table voix
→ RIFF
```

#### Objectifs

```text
état groupe/objet ou temps
→ condition trigger type 1/2/4/5/9
→ effet
→ statistique MissionManager
→ consommation HSM
```

#### HUD

```text
OrderProperty / changement d'état
→ objet actif et enfants
→ manager global, vslot +0x54
→ pipeline de présentation
```

Le nom de la classe et du widget concret reste ouvert.

#### HSM

```text
services + statistiques + descripteur SubMis
→ demo:0x822100A8 / demo:0x82211B88
→ curseur soumission/descripteur
→ transition de phase
```

#### Return Line

Le troisième descripteur de phase contient :

```text
borne générale : -50000
ligne distincte : -37000
temps : 300 secondes
flags : 0x251
selector : 0x00020001
```

Pour fermer définitivement la géométrie, il reste à qualifier le lecteur des
huit flottants et l'ordre des axes.

---

## 6. Fermeture des frontières résiduelles

### AC6 démo — fermeture des frontières résiduelles de mission

Date : 2026-08-17  
Portée : analyse statique PPC et données de mission

#### Résumé

Les frontières résiduelles sont désormais fermées au niveau opérationnel :

- la Return Line est un prédicat X/Z exact ;
- le pont HUD est rattaché au `CAce6HudImpl` embarqué dans le MissionManager ;
- `RadioTblBin` emploie des IDs de lignes vocales, pas les IDs des 229 événements globaux ;
- les services Strigon `219..226` sont des latches de synchronisation/cleanup ;
- les services Nimbus `243..251` et `293` commandent des Acts de nettoyage ;
- le writer du service `294` appartient au cycle de vie d'une lecture radio campagne ;
- le service `319` est inutilisé/orphelin dans la démo.

#### 1. Return Line : layout et consommateur

`demo:0x82211A38` lit le descripteur SubMis et publie quatre limites :

```text
manager+0x2C8  minX
manager+0x2CC  minZ
manager+0x2D0  maxX
manager+0x2D4  maxZ
```

Le prédicat `demo:0x82211AB0` évalue :

```cpp
minX <= x && x <= maxX &&
minZ <= z && z <= maxZ;
```

Si aucun point explicite n'est fourni, il récupère l'objet sélectionné dans le
UnitManager et lit :

```text
object+0x50  X
object+0x58  Z
```

Le descripteur terminal configure :

```text
zone active : X,Z dans [-50000, 50000]
ligne :       x = -37000, z dans [-50000, 50000]
temps :       300 secondes
flags :       0x251
selector :    0x00020001
```

La ligne de retrait est donc exactement la frontière verticale `x=-37000` dans
le plan de mission. L'étiquette narrative « Return Line ouest » reste B+ car
elle dépend de l'orientation du monde documenté, pas d'un nom stocké dans le
binaire.

#### 2. Pont HUD corrigé

L'ancien pont direct `OrderProperty → MissionManager vslot +0x54 → HUD` est
réfuté pour le manager campagne :

```text
demo:0x820AC748  blr
```

`OrderProperty` modifie bien l'objet courant et ses enfants, mais ce callback de
mode est un no-op.

Le vrai HUD est embarqué dans le MissionManager :

```text
MissionManager+0xCE10  ACE6::CAce6HudImpl<CX360HudRenderer>
MissionManager+0x2D8   pointeur publié vers cet objet
```

Le chemin per-frame est :

```text
demo:0x822158B0  UpHud
→ manager+0x2D8
→ vslot +0x38
demo:0x821FE2E0
```

`demo:0x821FE2E0` est un dispatcher sur huit modes HUD. Il obtient une
sous-interface via son vslot `+0x20`, puis appelle le vslot `+0x38` de cette
interface avec un index 0..7.

Le pont MissionManager → HUD générique est fermé. Les noms des huit
sous-interfaces ou widgets restent ouverts.

#### 3. Namespace de RadioTblBin

Le `RadioTblBin` contient :

```text
49 records avec un line_id valide
4 records terminaux line_id = 0xFFFF
```

Les 49 IDs se joignent exactement à la table des 738 voix de `entry10 child14` :

```text
575 → v066_1004
243 → v018_1025
265 → v038_1022
199 → v005_1525
736 → v219_1001
```

Les quatre records `0xFFFF` sont des contrôles conditionnés par le service
`325`, valeurs 1..4. Ils n'ont pas de voix directe.

La table `entry10 child15` de 229 noms sémantiques appartient à un namespace
séparé. L'égalité accidentelle de certaines valeurs n'est pas une relation.

La chaîne correcte est :

```text
services de mission
→ condition RadioTblBin
→ radio line_id
→ vNNN_NNNN
→ voicepack ENG/JPN
→ RIFF XMA1
```

#### 4. Strigon : services 219..226

Les huit groupes Su-33 ont un Act secondaire `Disappear → Jump` conditionné
par :

```text
165 → service 219 == 1
166 → service 220 == 1
167 → service 221 == 1
168 → service 222 == 1
169 → service 223 == 1
170 → service 224 == 1
171 → service 225 == 1
172 → service 226 == 1
```

Les mêmes groupes écrivent ces services dans leurs scripts tactiques.

La sémantique minimale est donc :

```text
219      latch partagé de synchronisation/fin d'élément
220..226 latches individuels de complétion et nettoyage
```

Ils ne correspondent ni à une seconde vague ni au retrait global.

#### 5. Nimbus : 243..251, 293 et 294

Les services `243..251` sélectionnent à la valeur zéro les Acts
`Disappear → Jump` des paires :

```text
243 → 191 et 200
...
251 → 199 et 208
```

Ce sont des latches d'activité par canal :

```text
non-zéro → canal actif
zéro     → cleanup des deux objets associés
```

Le service `293`, produit par `258==1 && 259==1`, sélectionne les Acts de
nettoyage pour `182..199` et le contrôleur `209`. C'est une première barrière
de cleanup global.

Le service `294` sélectionne une seconde barrière pour une partie de ces
objets. Son writer est maintenant rattaché à la radio campagne.

##### Writer du service 294

Le helper :

```text
demo:0x8223AB70
```

lit dans un record de lecture radio :

```text
raw_record+0x0E  service_id
raw_record+0x10  valeur
raw_record+0x2C  mode d'écriture
```

puis appelle :

```text
demo:0x82210190
```

Son caller direct est `demo:0x8223B648`, dans le cluster de méthodes du
`CX360RadioPlayManagerCampaign` (vtable `0x820022E4`, méthode principale autour
de `demo:0x8223B4C0`).

La frontière d'ownership est donc fermée :

```text
cycle de vie d'une lecture radio campagne
→ metadata de fin du record
→ service 294
→ cleanup Nimbus B
```

Le record radio exact qui porte `294` n'a pas encore reçu de ligne vocale ou de
nom humain. C'est une frontière de données lexicales, pas un writer externe
inconnu.

#### 6. Service 319

Le service `319` est déclaré, mais l'analyse exhaustive ne trouve :

- aucun trigger ;
- aucun `OrderFlag` ;
- aucune condition de démarrage d'Act ;
- aucun consommateur radio.

Il est donc classé **réservé, orphelin ou inutilisé dans cette démo**. Un
breakpoint supplémentaire ne transformerait pas nécessairement une absence en
fonctionnalité, même si cette stratégie a parfois beaucoup de succès en réunion.

#### Matrice finale

| Front | Verdict |
|---|---|
| axes et prédicat de Return Line | fermé A- |
| label narratif Return Line ouest | B+ |
| MissionManager → HUD générique | fermé A |
| `OrderProperty → HUD direct` | réfuté |
| `RadioTblBin` | 49 lignes exactes + 4 contrôles `0xFFFF`, fermé A |
| services Strigon 219..226 | synchronisation/cleanup fermés A |
| services Nimbus 243..251/293 | cleanup fermé A/A- |
| service 294 | writer owner fermé A-, record exact ouvert |
| service 319 | unused/orphan fermé négativement A |

---

## 7. Corrections et rétractations

### AC6 démo — corrections et rétractations

#### `OrderProperty → HUD`

Rétracté. Le vslot `+0x54` du MissionManager campagne est un `blr`.
`OrderProperty` modifie l'état de l'objet et de ses enfants, mais ne constitue
pas le pont HUD direct.

Le pont correct est :

```text
MissionManager+0x2D8
→ CAce6HudImpl<CX360HudRenderer>
→ vslot +0x38
→ demo:0x821FE2E0
```

#### IDs de `RadioTblBin`

Rétracté : les IDs ne sont pas des événements radio globaux.

Correction :

```text
49 IDs → table de voix child14
4 IDs 0xFFFF → records de contrôle sans voix
child15 → namespace distinct de 229 événements sémantiques
```

#### Nimbus 243..251

Rétracté : ces services ne sont pas les portes d'activation initiale.

Correction : la valeur zéro sélectionne des Acts `Disappear+Jump`. Ce sont des
latches d'activité dont l'effacement provoque le cleanup.

#### Service 294

Ancien statut : writer externe inconnu.

Correction : le helper `demo:0x8223AB70`, appelé dans le cluster
`CX360RadioPlayManagerCampaign`, lit l'identifiant, la valeur et le mode depuis
un record radio, puis appelle `demo:0x82210190`. Le propriétaire du writer est
fermé ; seul le record source exact reste à étiqueter.

---

## 8. Débriefing et replay tactique

### AC6 démo — attaque du sous-système de débriefing et replay tactique

Date : 2026-08-17  
Portée : analyse statique PPC/RTTI, sans exécution runtime

#### Résumé

Le sous-système de débriefing n'est pas une simple page de score. Il est un
**lecteur temporel de replay tactique** qui :

- suit des entités dans un pool de records de `0x150` octets ;
- consomme des événements de création et de mise à jour ;
- maintient une timeline, des keyframes et une vitesse de lecture ;
- met à jour une caméra dédiée et plusieurs sous-composants ;
- agrège séparément des événements statistiques.

Le calcul de classement est un sous-système distinct, porté par
`CModeTaskRanking`. Une page affichée après la mission n'est donc pas une classe
unique, même si l'interface utilisateur encourage fortement cette illusion.

#### 1. Topologie RTTI

Les classes exactes retrouvées sont :

```text
CDebriefingCamera
CDebriefingManager
CModeTaskDebriefing
CModeTaskDebriefingOnline
CModeTaskRanking
```

`CModeTaskDebriefing` possède un sous-objet secondaire à l'offset `+0x68` et
embarque un `CDebriefingManager` à partir de `task+0x70`.

Le constructeur principal est :

```text
demo:0x82170730
```

Il appelle le constructeur du manager :

```text
demo:0x8211DCA8
```

Le manager contient notamment :

```text
+0x030  CDebriefingCamera
+0x130  helper temporel / vue A
+0x2BC  helper temporel / vue B
+0x448  composant de présentation
+0xB44  composant auxiliaire
```

#### 2. Producteur gameplay et handoff

Le chargement de mission contient une phase nominale :

```text
Debriefing Record
```

Elle initialise un recorder gameplay de grande taille autour de :

```text
mission_root+0xC07898
```

et publie un pointeur global associé autour de :

```text
mission_root+0xC07640
```

Le producteur gameplay et le lecteur de débriefing sont tous deux bornés. La
relation directe entre leur champ global exact et l'initialisation du manager
de débriefing reste partielle ; elle ne bloque plus l'analyse interne du format.

#### 3. Pool d'entités suivies

La fonction :

```text
demo:0x8211DD10
```

gère un pool de records :

```text
manager+0x468  capacité
manager+0x46C  nombre de records
manager+0x474  base du pool
stride         0x150 octets
```

L'événement de type `34` crée ou enregistre une entité suivie. Les clés
principales du message sont :

```text
input+0x0C  identifiant d'entité
input+0x12  type d'événement
input+0x13  sous-type / catégorie
```

Le record conserve notamment un pointeur lié à `record+0x144`.

L'événement `35`, traité par `demo:0x8211DF28`, met à jour une entité existante
à partir du temps `input+0x08`, des flags `input+0x14` et d'un callback du
manager.

#### 4. Layout minimal d'un record `0x150`

Les champs fermés au niveau minimal sont :

| Zone | Rôle |
|---|---|
| `+0x000..` | état et échantillons temporels de l'entité |
| `+0x00C` | clé ou identité source |
| `+0x013` | catégorie compacte |
| `+0x144` | lien vers objet/track auxiliaire |
| `+0x14C` | état terminal ou bookkeeping de fin |

Le layout complet doit rester typé par les consommateurs. Le record contient
plus qu'un transform brut : il alimente interpolation, visibilité, relation de
cible et présentation.

#### 5. Événements agrégés

`demo:0x8211DE50` traite des événements qui ne créent pas directement une
entité suivie :

| Type | Handler | Effet minimal |
|---:|---|---|
| 3 | `demo:0x82275468` | incrémente un compteur catégoriel |
| 13 | `demo:0x82275430` | ajoute `input+0x14` à un total |
| 37 | `demo:0x822752D0` | met à jour des tables agrégées de 128 slots |
| 38 | `demo:0x82275450` | publie l'état compact `2` |

Les destinations comprennent :

```text
manager+0x618         total ou compte agrégé
manager+0x654         état compact
manager+0x658         somme
manager+0x65C..0x670  compteurs catégoriels
```

Leurs noms humains exacts restent ouverts. Ils appartiennent au replay et à
ses statistiques, sans prouver à eux seuls une formule de classement.

#### 6. Timeline de lecture

Le lecteur principal est :

```text
demo:0x8211E188
```

Il :

1. appelle les vslots `+0x70`, `+0x68` et `+0x6C` du manager avec le temps
   courant ;
2. parcourt tous les records `0x150` ;
3. appelle `demo:0x8212FE48` sur chaque track ;
4. met à jour les helpers à `+0x130`, `+0x2BC` et `+0x448` ;
5. avance le temps de `delta × playback_speed` ;
6. gère keyframes, fin, rewind ou reset.

Champs de timeline :

```text
manager+0x490  temps de reset / début de boucle
manager+0x494  borne supérieure / fin
manager+0x498  temps courant
manager+0x49C  tableau de temps de keyframes
manager+0x4A0  nombre de keyframes
manager+0x4A4  index de keyframe courant
manager+0x4A8  vitesse de lecture
manager+0x4AC  activation des événements agrégés
```

#### 7. Caméra

`CDebriefingCamera` possède sept méthodes virtuelles qualifiées par RTTI. Elle
est embarquée dans le manager et mise à jour pendant la lecture.

La topologie est fermée ; les noms métier des sept modes ou méthodes restent
ouverts. Les consommateurs montrent cependant au minimum des routes de suivi,
de cible et de vue d'ensemble. Une future passe pourra les nommer sans toucher
au format du replay lui-même.

#### 8. Machine de mode

Le task de débriefing utilise :

```text
task+0x0C  état HSM / mode
task+0x44  compteur ou temporisation
task+0x70  manager embarqué
```

Fonctions :

```text
demo:0x82170828  initialisation
demo:0x821708E0  update du manager par vslot +0x84
demo:0x821709A8  HSM du mode de débriefing
```

#### 9. Séparation du classement

`CModeTaskRanking` est une classe distincte. Le texte `Aircraft Rank` apparaît
dans un chemin autour de :

```text
demo:0x82135430
```

La conclusion est nette :

```text
Debriefing = reconstruction / lecture / statistiques tactiques
Ranking    = calcul ou présentation du rang
```

La formule de score et de rang n'est pas incluse dans cette attaque. Ce sera le
front suivant le plus naturel si l'objectif est de reproduire toute la boucle
post-mission.

#### Verdict

| Front | État |
|---|---|
| classes et ownership du debriefing | fermé A |
| pool de records `0x150` | fermé A- |
| création/mise à jour types 34/35 | fermée A- |
| événements agrégés 3/13/37/38 | opérations fermées, noms ouverts |
| timeline et keyframes | fermées A |
| caméra dédiée | ownership fermé, modes partiels |
| handoff recorder gameplay → manager | borné, champ exact partiel |
| classement / score | séparé, non attaqué ici |

---

## 9. Prochain sous-système : classement

### AC6 — prochaine frontière après le débriefing

#### Décision

Le meilleur front suivant est le sous-système **Ranking / score post-mission** :

```text
statistiques mission et débriefing
→ CModeTaskRanking
→ score, rang avion et bonus
→ sauvegarde / progression campagne
```

#### Pourquoi

Le replay tactique est maintenant borné et explicitement séparé du classement.
Attaquer `CModeTaskRanking` permettra de fermer la boucle :

```text
combat
→ objectifs/statistiques
→ débriefing
→ rang
→ progression
```

Les premières cibles sont :

- RTTI et vtables de `CModeTaskRanking` ;
- chemin `Aircraft Rank` autour de `demo:0x82135430` ;
- lecteurs des compteurs `manager+0x618/+0x658/+0x65C..0x670` ;
- writers de score dans les données de campagne et de sauvegarde.

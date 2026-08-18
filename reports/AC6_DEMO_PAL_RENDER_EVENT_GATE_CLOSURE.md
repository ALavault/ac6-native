# AC6 démo PAL — fermeture statique de la porte de soumission du renderer

Date : 2026-08-18  
Cible : démo campagne PAL Xbox 360, `Default.xex`  
XEX SHA-256 : `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`  
Image mémoire SHA-256 : `b81941994944b84f5792fd7b89cd698ca429b13c1bb4f501ea12e49dc54c2f01`

## Verdict

La soumission par trame n'est pas bloquée d'abord par un `KeSetEvent` arbitraire.
Elle est désactivée plus en amont par une porte explicite du device :

```text
device+0x5460
```

La seule fonction qui arme cette porte est :

```text
demo:0x821ADAB8
```

et le payload exact qui l'active est :

```text
event = 17
channel = 6
```

Ce callback est enregistré pendant l'initialisation du renderer par :

```text
demo:0x821ADC78
```

au moyen du service générique `47`, avec le descripteur :

```text
type = 2
callback = demo:0x821ADAB8
```

La frontière restante n'est donc plus « trouver un writer du champ » ni
« trouver quel `KeSetEvent` réveiller ». C'est :

```text
registre de callbacks, catégorie 2
→ livraison de l'événement (17, 6)
→ callback 0x821ADAB8
```

## 1. Contrat de `0x821ADAB8`

La fonction est bornée par `.pdata` :

```text
0x821ADAB8..0x821ADC77
112 instructions
SHA-256 des bytes :
723dcd6f1680e5bfa22657510176790ee9fec54c301b3d651a07a468ca4fdc85
```

### Événement 17

```powerpc
0x821ADB14  cmplwi event, 17
0x821ADB24  cmplwi channel, 6
0x821ADB40  stw 1, device+0x5460
0x821ADB44  stw 0, device+0x5458
0x821ADB48  stw 0, device+0x545C
```

Il :

1. ajoute le bit `1 << channel` au masque global `0x827AD2F4` ;
2. pour le canal 6, arme la soumission ;
3. remet à zéro les deux index de file.

### Événement 16

Il retire le bit du masque global et, pour le canal 6 :

```text
device+0x5460 = 0
```

### Événements 0 et 1

Ils remettent à zéro :

```text
masque global
device+0x5460
```

### Événement 34

Il publie la valeur de canal dans :

```text
device+0x5490
```

### Événements 224, 225 et 226

Ils suivent une voie de maintenance ou de rebind du device.

### Événement 255

Il rejoue les 17 entrées actives de la table `0x823C2EA8`. Cette voie est un
resynchroniseur de l'état des canaux, pas l'armement initial à elle seule.

## 2. Enregistrement du callback

`demo:0x821ADC78` est bornée par :

```text
0x821ADC78..0x821ADD8F
70 instructions
SHA-256 :
22ea5482a6bb9266f0de8a4bf275476efc6be36efbcaa2ad15ef724bcd4a721a
```

Elle construit sur la pile :

```text
+0x50  type 2
+0x54  callback 0x821ADAB8
```

puis appelle l'interface générique avec :

```text
service = 47
descriptor = &stack[0x50]
```

Le propriétaire est l'initialiseur du renderer :

```text
demo:0x821C64E8..0x821C6703
appel à 0x821ADC78 : 0x821C667C
```

Le même service `47` est utilisé par le sous-système audio :

```text
type 4, callback 0x82369290
type 4, callback null
```

Cela ferme son rôle minimal :

> service 47 enregistre ou retire un callback par catégorie.

Le nom original de l'API reste inconnu. Le contrat d'appel ne l'est plus.

## 3. Consommation de la porte

La fonction par trame :

```text
demo:0x821C57D0
```

lit la porte à :

```text
0x821C5878  lwz r7, device+0x5460
```

Elle ne construit un paquet que si :

```text
device+0x5460 != 0
ET
distance entre les index de file < 6
```

La construction est effectuée par :

```text
demo:0x821ADD90
appel à 0x821C5918
```

L'événement `(17,6)` remet justement les index à zéro avant d'armer la porte.
Cette relation exclut l'hypothèse d'un simple booléen de debug ou d'un résidu
non consommé.

## 4. Recensement de `KeSetEvent`

L'import est :

```text
demo:0x823760F4
xboxkrnl.exe!KeSetEvent
```

Dix callsites `bl` directs existent dans tout le XEX, plus un helper renderer
en tail-call.

### Renderer

```text
0x821C4AD0  enqueue courant puis signal de l'événement worker
0x821C4AE8  helper tail-call équivalent
0x821C4FD4  signal des workers pendant l'arrêt
```

Le processeur courant est choisi par :

```text
lbz ..., 0x10C(r13)
```

et indexe six records de `0x50` octets.

Le processeur de paquets `0x821BD970` atteint le helper `0x821C4AE8` à
`0x821BDC6C`, mais seulement après qu'un paquet a été produit.

### Non-renderer

```text
0x82338C6C
0x82338DC0
0x82338E34
0x82338EE0
```

appartiennent à une famille de synchronisation I/O générique.

```text
0x82355128
0x8235587C
0x82355D90
0x82355EA8
```

appartiennent au cluster XAudio.

La question « lequel des dix `KeSetEvent` est celui du renderer ? » est donc
fermée. Surtout, les appels renderer se trouvent **en aval** de la porte
`device+0x5460`.

Forcer un événement worker sans livrer `(17,6)` traiterait un symptôme et
court-circuiterait le contrat de file. Les logiciels adorent ce genre de
solution, surtout lorsqu'il faut ensuite expliquer les races qu'elle produit.

## 5. Frontière native exacte

Les observations existantes sur `main` montrent :

```text
device+0x5460 = 0 pendant toute la route mesurée
0x821ADAB8 jamais atteint nativement
0x821ADAB8 exécuté par l'oracle
```

La fermeture statique ajoute maintenant :

```text
seul writer
+ payload d'activation exact
+ enregistrement du callback
+ catégorie du registre
+ propriétaire renderer
```

La prochaine capture minimale sous l'oracle est :

```text
première entrée dans 0x821ADAB8
LR
r3 = event
r4 = channel
pile 0x100 octets
objet ou dispatcher ayant chargé le callback
tick
```

La valeur attendue pour l'armement est `(17,6)`. Le caller ou dispatcher
observé fermera le dernier owner du service 47.

## Verdict

| Claim | État |
|---|---|
| writer de `device+0x5460` | **fermé A** |
| payload d'armement `(17,6)` | **fermé A** |
| callback enregistré par le renderer | **fermé A** |
| service 47 = registre par catégorie | **fermé A-** |
| callsites `KeSetEvent` renderer | **fermés A** |
| `KeSetEvent` manquant = cause amont | **réfuté** |
| dispatcher concret qui livre `(17,6)` | **ouvert, capture bornée** |

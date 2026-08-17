# AC6 démo — résultat terminal et checkpoint de phase

Date : 2026-08-17  
Corpus : démo Xbox 360 `ACE6_X360`  
Méthode : analyse statique sur l'image mémoire corrigée, sans Xenia Edge

## Résumé exécutif

Deux frontières réellement atteignables dans la démo sont maintenant fermées :

```text
MissionManager terminal state
→ CModeTaskGameDemoOffline result 0/1/2
→ Title ou Advertise
```

et :

```text
transition SubMis
→ armement du checkpoint mode 2
→ copie d'un registre de blocs gameplay
→ failed-menu retry
→ restauration brute
→ fixups et rebinds
```

Le résultat 0 est l'abandon explicite vers le titre. Le résultat 1 est la sortie
terminale ordinaire vers l'écran publicitaire. Le résultat 2 est une route
spéciale compilée pour la mission finale du jeu complet : son prédicat exige
l'index de mission 15, donc elle n'est pas atteignable par la démo Mission 01.

Le checkpoint est un rollback **dans la même session**, pas une savestate
portable. Il copie des objets contenant des pointeurs et suppose que leurs
allocations restent vivantes aux mêmes adresses.

## 1. Périmètre de la démo

Le graphe qualifié `DemoOffline` contient :

```text
StartUp → Title → Loading → Game
Game result 0 → Title
Game result 1/2 → Advertise → Title
```

Les classes Hangar, Gallery, Ranking et Debriefing sont compilées dans le XEX
partagé mais ne sont pas atteignables par ce graphe. Elles restent de
l'archéologie compiled-only et ne doivent pas être transformées en UX de la
démo par enthousiasme rétroactif.

## 2. Producteur des résultats GameDemoOffline

### 2.1 MissionManager vers outcome interne

`demo:0x8217B848` attend la phase MissionManager `14`, puis lit :

```text
MissionManager+0x338
```

et produit l'outcome interne à :

```text
CModeTaskGame+0x80
```

Contrat minimal :

| État manager | Outcome interne |
|---:|---:|
| 0 | 0 |
| 1 | 1 ; variante 5 si le mode application vaut 5 ; variante 3 seulement si mission=15 et mode application=1 |
| 2 | aucune route terminale observée dans ce sélecteur |
| 3 | 2 |
| 4 | 1 |
| 5 | 1 |

### 2.2 Outcome interne vers résultat de tâche

`demo:0x82174678` applique :

```text
outcome 1 → result 1
outcome 2 → result 0
outcome 3 → result 2
outcome 5 → result 1
autre outcome → callback spécial, hors table ordinaire
```

La surcharge `CModeTaskGameDemoOffline` `demo:0x82173D88` route ensuite :

```text
result 0 → CModeTaskTitleDemoOffline
result 1 → CModeTaskAdvertiseDemoOfflineEnd
result 2 → CModeTaskAdvertiseDemoOfflineEnd
```

### 2.3 Étiquettes fermées

`demo:0x82173A30` écrit explicitement :

```cpp
MissionManager+0x338 = 3;
```

avant d'appeler le vslot de sortie. La commande 4 du menu mission fait la même
chose. Cette voie devient :

```text
state 3 → outcome 2 → result 0 → Title
```

Le résultat 0 est donc le résultat **quit/abort vers le titre**.

Dans le finaliseur de mission `demo:0x822116F0`, le store à `demo:0x82211714` écrit :

```cpp
MissionManager+0x338 = 1;
```

puis calcule les rangs d'objectifs, appelle `demo:0x822771A0` et déclenche la
sortie. Cette voie devient :

```text
state 1 → outcome 1 → result 1 → Advertise
```

Le résultat 1 est donc la **fin de mission ordinaire** de la démo.

Le résultat 2 demeure une fin spéciale vers Advertise, mais son prédicat est
désormais borné : `mission_index == 15 && application_mode == 1`. Il appartient
au code partagé du jeu complet et ne peut pas être produit par Mission 01 de la
démo. Il ne faut donc surtout pas le baptiser `demo failure`.

## 3. Moteur de snapshot

L'objet snapshot possède :

```text
+0x08  buffer contigu de snapshot
+0x0C  tableau de descripteurs {live_pointer, size}
+0x10  nombre de descripteurs
+0x18  taille cumulée
```

`demo:0x822174A0` ajoute un descripteur.

`demo:0x822173B8` sauvegarde :

```cpp
for descriptor in registry:
    memcpy(snapshot_cursor, descriptor.live_pointer, descriptor.size);
```

`demo:0x82217428` restaure l'opération inverse.

Le moteur ne sérialise aucun nom, type ou relocalisation. C'est un copier-coller
ordonné d'octets vers des adresses déjà connues.

## 4. Inventaire des blocs qualifiés

### MissionManager

`demo:0x82296A40` enregistre :

1. le préfixe du MissionManager, taille `0xCE10` ;
2. le tableau de records runtime de 68 octets ;
3. le registre des services de 20 octets ;
4. une table d'indices/pointeurs de quatre octets ;
5. deux mots globaux de quatre octets.

Pour Mission 01, la partie fixe et les trois tableaux observés donnent un
minimum non-unités de :

```text
0xCE10
+ 361 × 68
+ 327 × 20
+ 213 × 4
+ 4 + 4
= 84 700 octets
```

Ce total n'inclut pas les objets d'unités, dont le nombre dépend de la route.

Le préfixe s'arrête exactement à `MissionManager+0xCE10`, là où commence l'objet
HUD embarqué. Le HUD est donc explicitement hors de la copie brute du manager.

### Unités

Les wrappers de factory `demo:0x820A3BD8..0x820A4430` enregistrent, lorsque le
flag de snapshot est actif, l'objet runtime entier avec sa taille d'allocation.

Tailles qualifiées :

```text
0x100, 0x200, 0x230, 0x270, 0x370, 0x380, 0x390, 0x3D0,
0x2830 et 0x2DA0
```

Ces blocs contiennent vtables et pointeurs. Leur restauration n'est sûre que
tant que le même graphe d'allocations existe encore.

## 5. Armement du checkpoint

`demo:0x82211B88` exécute le descripteur de sous-mission. Lorsque le chemin
qualifié demande un checkpoint, il appelle :

```text
demo:0x8220FE18
```

qui effectue :

```cpp
MissionManager+0x2BC = 2;
snapshot.save();
```

Le failed menu transmet la commande 2. `demo:0x8229ECE0` réutilise alors la
valeur `MissionManager+0x2BC`, donc le payload `2`, vers l'état
`demo:0x8229E698`.

Le restart complet transmet au contraire le payload `1` et contourne la branche
snapshot.

## 6. Fixups après restauration

Le mode 2 de `demo:0x8229E698` ne s'arrête pas au `memcpy` inverse. Il :

- récupère l'objet sélectionné dans le UnitManager ;
- effectue un handoff de transform autour de `object+0x50` et des champs manager
  `+0x348/+0x350` ;
- valide ou rebinde un objet d'état via `demo:0x82298100` ;
- reprend cet état par `demo:0x8217BC38` ;
- efface des bits de transition dans deux blocs manager ;
- réarme le MissionManager via `demo:0x82297F08` ;
- republie les états nécessaires aux sous-systèmes.

Le checkpoint possède donc deux couches :

```text
1. snapshot brut des blocs enregistrés
2. fixups et rebinds post-restore
```

Une reproduction qui ne ferait que restaurer les octets serait incomplète.

## 7. Ce que le checkpoint ne couvre pas

Aucune inscription directe au registre qualifié n'ajoute :

- l'état GPU ;
- l'état kernel ou des threads ;
- les contextes XMA et l'état du décodeur audio ;
- l'état hôte de Xenia ;
- l'objet HUD embarqué après `+0xCE10`.

Cela ne signifie pas que tous ces systèmes repartent de zéro. Certains sont
recalculés ou réarmés par les fixups. Cela signifie qu'ils ne font pas partie du
blob de snapshot gameplay.

## 8. Conséquence pour Xenia Edge

Le checkpoint interne peut servir de primitive de rollback de gameplay dans une
session intacte. Il ne peut pas être écrit sur disque puis restauré après un
redémarrage de l'émulateur sans :

- relocaliser les pointeurs ;
- recréer le graphe d'allocations ;
- restaurer les états kernel, GPU et audio ;
- exécuter les fixups dans le même ordre.

La stratégie minimale pour l'automatisation reste donc :

```text
session Xenia vivante
→ armement d'un checkpoint interne
→ pause hôte
→ observation / action
→ commande retry mode 2
```

et non une prétendue savestate générale obtenue par un bouton de debugger.

## Verdict

| Front | Verdict |
|---|---|
| result 0 → quit/abort → Title | **fermé A** |
| result 1 → fin ordinaire → Advertise | **fermé A-** |
| result 2 → route finale compilée → Advertise | **fermée A ; non atteignable par Mission 01** |
| snapshot save/restore générique | **fermé A** |
| inventaire MissionManager | **fermé A/A-** |
| inscription des objets d'unités | **fermée A** |
| checkpoint armé aux transitions SubMis | **fermé A-** |
| fixups post-restore | **fermés A-/B+** |
| checkpoint portable entre processus | **réfuté** |
| savestate Xenia complète | **hors contrat du checkpoint** |

## Audit adversarial

- Les nombres 361 et 213 sont ceux observés pour Mission 01 ; les tailles sont
  dérivées dynamiquement dans le code.
- Les deux mots globaux enregistrés ne reçoivent pas de noms métier inventés.
- Le résultat 2 n'est pas une branche de la démo : son prédicat exige la mission 15.
- L'absence d'inscription GPU/audio est une borne négative sur ce registre, pas
  une preuve que les sous-systèmes ne sont jamais réinitialisés.
- Les callsites compiled-only de galerie/sauvegarde ne sont pas promus dans
  l'inventaire du checkpoint DemoOffline.

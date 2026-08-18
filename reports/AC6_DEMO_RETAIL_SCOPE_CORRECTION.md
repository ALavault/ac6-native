# AC6 démo PAL — portée démo, code partagé et valeur retail

Date : 2026-08-18  
Cible : démo campagne PAL Xbox 360

## Correction

Un sous-système qui n'est pas atteint par le graphe `DemoOffline` n'est pas du
code perdu. Le XEX de la démo contient une partie substantielle du code partagé
avec le retail. La portée doit donc être décrite par trois propriétés
indépendantes :

1. le code est compilé dans le XEX partagé ;
2. le code est atteignable depuis le parcours PAL qualifié de la démo ;
3. le code fournit une ancre sémantique transférable vers le retail.

Confondre les deux dernières propriétés ferait disparaître précisément les
classes les plus utiles au rapprochement inter-build. Ce serait une méthode
remarquablement efficace pour jeter l'information que l'on vient d'extraire.

## Classification retenue

```text
demo-active
    compilé et relié au graphe DemoOffline qualifié

shared-retail-anchor
    compilé dans le XEX de la démo, non relié au graphe DemoOffline qualifié,
    mais utile pour nommer et reconstruire le retail

shared-gameplay-conditional
    consommateur générique compilé et relié au gameplay, mais producteur ou
    condition d'activation non prouvé pour Mission 01

unknown
    présence ou relation non encore qualifiée
```

## Conséquences

Les familles suivantes restent dans le corpus de travail :

```text
CModeTaskHangar
CModeTaskGallery*
CModeTaskRanking
CModeTaskDebriefing
CModeTaskMissionSelect
CModeTaskDemoIntermission
```

Le graphe `DemoOffline` ne les rend pas accessibles à l'utilisateur de la démo,
mais leurs RTTI, hiérarchies, vtables, constructeurs et consommateurs sont des
preuves du code partagé. Elles doivent être utilisées pour :

- transférer des noms vers le retail ;
- comparer les dispositions d'objets entre builds ;
- retrouver des consommateurs absents de Mission 01 ;
- distinguer une primitive générique du moteur d'une spécialisation de mission ;
- qualifier les différences demo/retail sans projeter aveuglément les adresses.

## Cas du menu supply

Le chemin générique est compilé et fermé statiquement :

```text
CSelectGameMenuManager, requête 3
→ GameMenu_OpenSupplyMenu
→ sélection 0
→ MissionManager, commande 5
```

La commande 5 choisit ensuite une branche selon `manager+0x3AC` et atteint soit
`demo:0x8229E158(this, 2)`, soit l'état `demo:0x8229CDF8`.

Ce résultat prouve un consommateur générique de supply dans le code partagé. Il
ne prouve pas, à lui seul, que Mission 01 produit la requête 3, ni que le mot
`supply` désigne une séquence de ravitaillement aérien. Le classement correct
est donc :

```text
compiled: yes
generic consumer: qualified
Mission 01 producer: not established
aerial-refuelling interpretation: not established
retail value: high
```

## Règle de transfert vers le retail

Une correspondance retail doit être promue seulement après au moins un des
contrôles suivants :

- signature de fonction ou de bloc identique ;
- RTTI et hiérarchie compatibles ;
- même factory key et même contrat d'appel ;
- même disposition d'objet démontrée par plusieurs consommateurs ;
- observation dynamique convergente.

Une adresse de vtable provenant d'un autre build n'est jamais, seule, une
preuve de classe. Les adresses ont cette fâcheuse habitude de changer quand on
recompile un programme.

## Verdict

| Question | Verdict |
|---|---|
| Code non atteint par `DemoOffline` perdu | **non** |
| Code non atteint attribuable au parcours utilisateur de la démo | **non sans preuve** |
| Code non atteint utilisable pour le retail | **oui, comme ancre qualifiée** |
| Supply générique compilé | **oui** |
| Supply produit par Mission 01 | **non établi** |
| Ravitaillement aérien dans Mission 01 | **non établi** |

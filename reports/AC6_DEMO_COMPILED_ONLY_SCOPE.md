# AC6 démo — périmètre des sous-systèmes compilés mais non atteignables

Date : 2026-08-17

## Règle

Le XEX de la démo partage du code avec le jeu complet. RTTI, vtables et factories
peuvent donc exister sans qu’aucune transition de la démo ne les rende
accessibles.

Le graphe qualifié `DemoOffline` contient seulement :

```text
StartUpDemoOffline
→ TitleDemoOffline
→ LoadingDemoOffline
→ GameDemoOffline
  ├── result 0 → TitleDemoOffline
  └── result 1 → AdvertiseDemoOfflineEnd → TitleDemoOffline
```

`result 2` est supporté par la table de sortie mais son producteur qualifié exige
la mission `15`; il est mort dans la démo Mission 01.

## Compiled-only

Les familles suivantes sont compilées mais absentes du graphe atteignable :

```text
CModeTaskHangar
CModeTaskGallery*
CModeTaskRanking
CModeTaskDebriefing
CModeTaskMissionSelect
CModeTaskDemoIntermission
```

Les rapports consacrés à ces classes décrivent de l’archéologie du binaire
partagé. Ils ne prouvent ni hangar, ni galerie, ni leaderboard, ni débriefing
accessible dans la démo.

## Conséquence de méthode

Toute promotion future doit fournir une arête de factory ou de transition depuis
une tâche `DemoOffline`. Une chaîne RTTI seule reste insuffisante. Cette règle
évite de transformer le code mort en interface utilisateur, activité déjà assez
répandue dans les roadmaps ordinaires.

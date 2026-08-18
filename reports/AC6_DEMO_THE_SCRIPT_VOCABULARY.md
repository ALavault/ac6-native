# Le vocabulaire de script du frontend, et la commande qui manque

Date : 2026-08-18

## La table

Le dispatcher `sub_820E8F90` marshalle les arguments d'après une signature
lue caractère par caractère — `'F'` 70, `'I'` 73, `'S'` 83 — puis appelle la
fonction native en `0x820E9130`. C'est un pont ActionScript → jeu.

Les fonctions natives sont enregistrées dans une table de quadruplets
`{nom, signature, type de retour, fonction}` à `0x82386408`, **40 entrées**.
Balayage indépendant de toute l'image sur la forme du quadruplet : 40
également, mêmes adresses.

## Ce que le film appelle réellement

Atlas de reachability, 12 000 ticks, deux routes :

| Adresse | Commande | Neutre | START |
|---|---|---|---|
| 0x820EA0A8 | `SetBuffer(II)->V` | oui | oui |
| 0x820EA128 | `PlayBGM(III)->V` | oui | oui |
| 0x820EA298 | `GetBGMTrackNo(I)->I` | oui | oui |
| 0x820EA4A8 | `menu_endMode(I)->V` | oui | oui |
| 0x820EA238 | `StopBGMFadeOut(II)->V` | **oui** | non |
| 0x820E9838 | `SendMsgI(S)->I` | non | **oui** |
| 0x820EA538 | `GetCurrentMode(V)->I` | non | **oui** |
| 0x820EA550 | `GetCurrentMission(V)->I` | non | **oui** |
| 0x820EA598 | `GetCurrentLevel(V)->I` | non | **oui** |
| 0x820EA6C0 | `sound_onVoice2D(S)->I` | non | **oui** |

## La commande qui fait avancer le titre

`menu_endMode` occupe `0x820EA4A8..0x820EA538`. Le site `0x820EA500`, qui
appelle le slot +0x54 des tâches de mode dans la chaîne du tick 4251, est
**dans son corps**. `menu_endMode` est donc le mécanisme de transition, et la
chaîne complète se lit :

```text
film ActionScript
→ sub_820E8F90        marshalle la signature
→ 0x820E9130 bctrl    -> menu_endMode  (0x820EA4A8)
→ 0x820EA500          -> slot +0x54, base des tâches de mode (0x8217C890)
→ 0x8217C8B8          -> slot +0x48 de CModeTaskTitleDemoOffline (0x8218AB98)
→ 0x8218A7F4          -> bras d'état 2 de CModeTaskTitle::update
```

## Ce que fait l'appui, et ce qu'il ne fait pas

Au tick 3001, avec START, le film joue un son de confirmation
(`sound_onVoice2D`), envoie un message (`SendMsgI`) et **interroge l'état du
jeu** : `GetCurrentMode`, `GetCurrentMission`, `GetCurrentLevel`. Puis il
n'appelle plus `menu_endMode`.

En neutre, à 4251, il appelle `StopBGMFadeOut` puis `menu_endMode` : c'est le
délai d'attract qui expire.

Le script, après l'appui, consulte donc l'état du jeu et **décide de ne pas
terminer le mode**. C'est la première fois de cette campagne qu'une décision
du jeu, et non une adresse morte, est le sujet.

## Où lire la réponse

Les trois accesseurs lisent le même singleton :

```text
[0x823C27E0]                     objet d'état de jeu
GetCurrentMode      = [gs+120]
GetCurrentMission   = sub_82095B80(gs+112), forcé à 16 si sub_820E9300(gs+112)
GetCurrentLevel     = sub_820E9290(gs+112), avec 6 et 7 échangés
```

## Non établi

- Les valeurs rendues à l'exécution. La mesure est instrumentée mais pas
  encore lue ; elle ne figure pas ici pour cette raison.
- L'argument chaîne de `SendMsgI`, qui est en mémoire invitée
  (`r4=0x7F040198`) et n'est pas dans le rapport.
- Si le refus vient d'une de ces trois valeurs ou d'autre chose.

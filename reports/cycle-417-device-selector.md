# Cycle 417 — l'écran bloqué est un sélecteur de périphérique, et il se termine

## 1. Ce que fait l'invité à l'instant du blocage

À `12:13:16.617`, exactement en arrivant sur l'écran :

```
[ac6-dialog] sub_821CE8A8 #1 obj=0xA3300060
XamShowDeviceSelectorUI(00000000, 00000001, 00000200, 0…0, A33000B8, A33000BC)
[ac6-dialog] XamShowDeviceSelectorUI called from guest 0x821CE928
```

**L'écran YES/NO est le sélecteur de périphérique de stockage d'AC6.** Ce n'est
donc pas l'écran de chargement de sauvegarde : le détecteur de l'écran, qui
reconnaît la bande de boutons, les confond. Les cycles 393 et suivants
appelaient « save-screen » ce qui est en réalité l'invite de sélection de
stockage.

## 2. La modale se termine correctement

`overlapped = 0xA33000BC` non nul, donc trajet différé. La suite du journal :

| horodatage | événement |
|---|---|
| .617 | `CompleteOverlappedDeferredEx: queuing for overlapped A33000BC` |
| .618 | `Deferred overlapped A33000BC: running pre_callback` |
| .618 | `Broadcasting XN_SYS_UI = true` → 3 auditeurs |
| .620 | l'invité reçoit `XNotifyGetNext → id=0x9, param=1` |
| .635 | deux autres fils de l'invité reçoivent la même notification |

`XN_SYS_UI` apparaît **2 fois** sur toute l'exécution : le `true` et le `false`.
La complétion différée a donc bien eu lieu, `device_id` vaut 1 et le résultat
est `SUCCESS`.

## 3. Le fait à expliquer

La modale se termine à `.7`, l'invité en est notifié, **et l'écran est toujours
figé à `12:13:31`**, quinze secondes plus tard.

Donc : le sélecteur n'est pas bloqué en attente. Il rend la main, l'invité reçoit
tout ce qu'il faut, et n'avance pas quand même.

## 4. Pistes, par ordre de coût

1. les champs de la structure `overlapped` — statut, longueur, code d'erreur —
   sont-ils écrits comme l'invité les lit ? Une complétion signalée sans champ
   correctement rempli produirait exactement ceci ;
2. `device_id = 1` est-il ensuite accepté ? Si l'invité interroge
   `XamContentGetDeviceState`/`GetDeviceData` sur ce périphérique et reçoit une
   incohérence, il peut renoncer sans rien afficher ;
3. `sub_821CE928`, le site d'appel, est le point d'entrée pour lire ce que
   l'invité fait du résultat.

## 5. Correction de nomenclature à propager

Le détecteur `ac6-detect-screen.py` doit être renommé ou requalifié : il détecte
l'invite de sélection de stockage, pas l'écran de sauvegarde. Tous les rapports
depuis le cycle 393 emploient le mauvais terme.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

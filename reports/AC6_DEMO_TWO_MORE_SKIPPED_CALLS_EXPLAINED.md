# Deux des cinq appels sautés, expliqués

Date : 2026-08-18

`e3b017db` a nommé cinq imports que le retail appelle et nous jamais, et
`460d1009` en a expliqué et corrigé un (`XGetAVPack`, garde
`XexCheckExecutablePrivilege`). Voici deux autres, avec des causes de nature
différente.

## `RtlImageXexHeaderField` — un import de données non patché

La garde, dans `sub_821A9140` (atteinte une fois) :

```c
r11 = [0x82000630];        // slot d'import XexExecutableModuleHandle, ordinal 403
r11 = [r11];
if (r11 == 0) goto loc_821A9198;     // saute
r3 = [r11 + 88];
r4 = 131072 | 1025;                  // = 0x00020401
RtlImageXexHeaderField(r3, 0x00020401);
```

`0x82000630` est **exactement** le slot que `12c5a372` a recensé parmi les dix
imports de données `CONST` non patchés. Il contient donc
`(1 << 16) | 403 = 0x00010193`, dont le déréférencement vaut 0, et la garde
saute.

Le log de l'oracle confirme l'argument attendu :
`RtlImageXexHeaderField(3001D000, 00020401)` à la frame 0. La clé `0x00020401`
est celle des informations TLS du XEX.

C'est le **deuxième** des dix imports de données à montrer une conséquence
mesurée — et le premier dont la conséquence diverge visiblement de l'oracle.
`KeTimeStampBundle`, réparé en `f0e1ad75`, n'avait rien changé.

## `KeResetEvent` — une attente qui expire

Aucune garde ici. Dans `sub_821C4970` (atteinte deux fois) :

```c
r5 = 1; r4 = 3; r3 = r29;
KeWaitForSingleObject(r29, 3, 1);
if (r3 == 258) goto loc_821C49D0;    // 258 = STATUS_TIMEOUT -> boucle
loc_821C4A30:
    KeResetEvent(r28 + 32);
```

`KeResetEvent` n'est pas sauté par un test sur une valeur non initialisée : il
est sauté parce que **l'attente expire**. L'événement n'est jamais signalé chez
nous, alors que l'oracle le réinitialise 2 246 fois — à partir de sa frame 3061,
pas avant.

Et `sub_821C4970` n'est pas quelconque : son LR `0x821C4A28` est l'un des deux
que la trace `AC6_RING_KICK` a enregistrés au tick 0. C'est la fonction qui
attend le GPU.

## Ce que cela dit des dix imports de données

Deux sur dix ont maintenant une conséquence lisible, et elles sont opposées :
`KeTimeStampBundle` réparé n'a rien changé ; `XexExecutableModuleHandle` non
réparé fait sauter un appel que l'oracle fait. Généraliser depuis dix cas dont
deux sont mesurés serait prématuré, et les huit autres restent sans conséquence
établie.

## Non établi

- Que réparer `XexExecutableModuleHandle` change quelque chose au-delà de faire
  appeler `RtlImageXexHeaderField`. Il faut un objet module dont le champ +88
  soit un en-tête lisible ; l'en-tête, lui, est dans le fichier.
- Quel événement `sub_821C4970` attend, et qui devrait le signaler.
- Pourquoi l'oracle ne commence à réinitialiser qu'à sa frame 3061.

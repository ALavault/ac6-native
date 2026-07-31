# Cycle 437 — mesuré : l'état vaut **0**, pas 2. Neuf cycles reposaient sur une erreur

## 1. Relevé, une fois par seconde pendant le blocage

```
[ac6-screen] obj=0xA3300060 state=0 device_id=0x00000001 ovl_result=0x00000000 …
```

Constant sur dix relevés consécutifs.

| champ | valeur | signification |
|---|---|---|
| `[obj+84]` | **0** | état **repos**, pas « en attente » |
| `[obj+88]` | **1** | `device_id` **bien reçu** |
| `[obj+92]` | 0 | `SUCCESS` |

## 2. Ce que cela réfute

Les cycles 428 à 436 reposaient tous sur une même prémisse : l'écran resterait
figé en **état 2**, faute que quiconque relise l'`overlapped`.

**C'est faux.** L'état vaut 0. Le sélecteur de périphérique s'est **entièrement
terminé**, l'invité a **accepté** `device_id = 1`, et la machine à états est
revenue au repos.

Il n'y a donc ni attente bloquée, ni complétion ignorée, ni scrutation
manquante. La « question réduite à un seul maillon » du cycle 434 portait sur un
maillon qui n'existe pas.

## 3. Conséquence sur le diagnostic

Le dialogue YES/NO affiché **n'est pas** le sélecteur de périphérique en
attente. Le sélecteur est fini. C'est **un autre écran**, et le cycle 417 —
« l'écran bloqué est un sélecteur de périphérique » — était une association
faite sur la seule coïncidence temporelle de l'appel.

## 4. Pourquoi l'erreur a duré neuf cycles

L'état n'a jamais été relevé. Il a été **déduit** : le lanceur pose 2 sur 997,
donc l'écran serait en 2. Le raisonnement était juste ; sa prémisse — que rien
ne remette l'état à 0 — venait d'une recherche textuelle dont le cycle 436 a
lui-même montré qu'elle ne prouvait rien.

Une seule lecture mémoire, faisable dès le cycle 428, aurait évité toute la
série. C'est la même leçon qu'aux cycles 412, 428, 431, 433 et 436, et je la
retiens mal : **relever avant de déduire**, surtout quand la valeur est à portée
d'une lecture.

## 5. État réel

Acquis et mesurés : l'entrée arrive intacte, la navigation fonctionne, la
validation ne fait rien, le sélecteur s'est terminé proprement, le stockage est
accepté.

Inconnu : **quel écran** est réellement affiché, et ce qu'il attend.

La reprise doit repartir de là — identifier l'écran par son propre code, pas par
un appel noyau voisin dans le temps.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

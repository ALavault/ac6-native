# Cycle 403 — aucun fil invité ne tourne en boucle ; tous dorment

## 1. Mesure

Échantillonnage `/proc/<pid>/task/*` en deux prises espacées de 4 s, sur
l'écran bloqué. Deux prises et non une : elles donnent le temps CPU consommé
dans l'intervalle, ce qu'un instantané ne peut pas fournir.

71 fils. Extrait par consommation décroissante :

| fil | état | ticks CPU / 4 s | wchan |
|---|---|---|---|
| Audio Worker | S | 395 | — |
| ac6recomp (principal) | R | 144 | `poll_schedule_timeout` |
| ac6recomp | S | 49 | `hrtimer_nanosleep` |
| **XThread85FFD6C0** | **S** | **30** | `futex_do_wait` |
| **XThread6087D6C0** | **S** | **28** | `futex_do_wait` |
| XThread5D8796C0 | S | 9 | `hrtimer_nanosleep` |
| ~40 autres XThread… | S | 0–2 | `futex_do_wait` |

## 2. Lecture

**Aucun fil invité n'est en état R.** Tous sont endormis, en attente de futex ou
en sommeil temporisé. Les deux plus actifs consomment 0,3 s de CPU sur 4 s, soit
environ 7 % — le profil d'un travail périodique par trame, pas d'une boucle
bloquée.

Cela **réfute la première des deux formes** retenues au cycle 402 : la machine à
états de l'interface ne tourne pas en boucle, puisque rien ne tourne. Il reste
la seconde — des fils en attente — mais avec une nuance importante : `futex_do_wait`
à 0 tick est aussi l'état normal d'un fil au repos. La mesure **ne montre pas de
blocage anormal**, elle montre un système au repos qui rend des trames.

Je ne conclus pas au blocage d'un fil précis : rien dans ces chiffres ne
distingue un fil en interblocage d'un fil simplement inutilisé.

## 3. Où cela laisse le diagnostic

Les deux formes du cycle 402 sont maintenant l'une réfutée, l'autre non établie.
L'invité rend, reçoit les entrées, n'appelle pas le noyau, et aucun de ses fils
ne travaille anormalement. C'est le profil d'un programme qui **a terminé sa
transition d'écran et attend** — sauf que l'écran attendu n'est dessiné qu'à
moitié et n'accepte rien.

La question utile n'est donc plus « qu'est-ce qui bloque » mais **« quel état
l'interface croit-elle occuper »**, ce qui se lit dans les données de l'invité,
pas dans l'ordonnanceur hôte.

## 4. Note d'outillage

`eu-stack -p` sur ce processus se bloque au déroulement de pile et ne rend pas
la main ; il a fait expirer une exécution complète. L'échantillonnage `/proc`
donne moins de détail mais coûte quelques millisecondes. À préférer ici.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

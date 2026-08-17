# Cycle 1728 — handoff d’événement PAL après XMA

## Verdict

Une sonde read-only du scheduler natif confirme, sur des probes neutral et
START identiques au cycle 1727, que la paire d’attente
`E0000048 -> E000004C` est effectivement réveillée. Entre les ticks 1040 et
1099, le thread 12 publie `E000004C` 60 fois et le waiter sélectionné est le
thread 1 à chaque fois. Le thread 1 reprend 60 fois, publie ensuite lui-même
`E000004C` sans waiter, puis ré-entre dans une seconde
`NtSignalAndWaitForSingleObjectEx`. Cette trace ne montre pas une perte de
réveil dans le scheduler natif; elle ne qualifie pas encore pourquoi le
thread 1 reste dans cette boucle ni la sémantique du code appelé.

Le résultat terminal reste `max_ticks=1100`, avec 23 threads bloqués, une
présentation et aucun frontend, pixel, audio ou mission. Aucun patch Xenia,
run Xenia ou `ptrace` n'a été utilisé pour cette mesure. Le run historique
`strace` de l'archive cycle 1725 reste un oracle séparé.

## Identité et exécution

| élément | valeur |
|---|---|
| cible | `Default.xex`, démo PAL, Xenon big-endian/Xenos |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| binaire codegen-ON SHA-256 | `8c2138cab60eb123b31e89852c4fc0af030b47fc5c32f1eee5b933881424c841` |
| commande | `probe --until frontend --max-ticks 1100 --backend headless` |
| hook | `AC6_DEMO_WATCH_EVENT_HANDOFF=1`, `..._FOCUSED=1` |
| stores | cycle 1727 neutral/start, importés séparément |
| sortie temporaire | `/fastdata/lavaulta/tmp/ac6-cycle1728-event.IddBKU` |
| rr/Xenia | rr non rejoué; Xenia et son patch non utilisés |

La sonde ne modifie aucun event, waiter, store ou état de production. Les
stderr neutral et START sont byte-identiques, SHA-256
`4eb3242c877b8645320e4e3ca25cfeefcdee69276b23cf3f3b05fd983b6b00a0`.
Les rapports et traces restent ceux du cycle 1727 :

| route | rapport | trace | résultat |
|---|---|---|---|
| neutral | `18c331a6171b89429f1c277d9aaa208cc78c8d0be3bd935f5f53d1e60158bc78` | `4123e4b5115d9b2518bbaf0baa74c23af2a85fcba3afd0e8f994627bfc5a0e9d` | `max_ticks`, 23 bloqués |
| START | `2c9790c056395dcada28185618e4d163f10b1262002a513bb88990eadb2265cf` | `a9e5e7c9fe0279b0da38fd480f08ccd66ab546101989b5e467f38ecd3ecae207` | `max_ticks`, 23 bloqués |

## Handoff observé

Les 600 lignes ciblées par route (`tick=1040..1099`) ont exactement la même
forme :

| opération | compte | thread(s) | détail |
|---|---:|---|---|
| `set_enter` / `event_wake` / `set_exit` | 120 chacun | 12 puis 1 | `E000004C`; thread 12 réveille 1; thread 1 publie sans waiter |
| `signal_wait_enter` | 120 | 1 | `signal=E0000048`, `wait=E000004C` |
| `signal_wait_resume` | 60 | 1 | waiter sélectionné `1` |
| `signal_wait_block` | 60 | 1 | seconde entrée de la même tick |

Les 60 réveils ont tous `LR=0x821A69CC` côté waiter; les 120 publications
`NtSetEvent` ont `LR=0x821A6AC4`. Après le réveil, le thread 1 effectue un
`set E000004C` avec `state=1`, puis la seconde entrée consomme ce passage et
retombe dans `signal_wait_block`. Cette lecture est une séquence observée,
pas une attribution de rôle métier aux threads ou aux handles.

Le frontier final reste `thread=1`, `tick=1100`, `LR=0x822E559C`, cible
indirecte `0x822F8848`, clé d'attente `E000004C`, 23/23 threads bloqués.

## Classification et suite

- **demo-qualified** : comptes, ticks, handles bruts, PC/LR, sélection du
  waiter, égalité neutral/START et absence de mutation par la sonde.
- **demo-observed** : boucle de handoff thread 12 → thread 1 → thread 1,
  sans perte de réveil visible dans cette fenêtre.
- **xenia-generic** : uniquement l'information historique que le patch Xenia
  corrige `SignalAndWait` POSIX; aucune règle n'est transplantée.
- **unknown** : cause de la ré-entrée, contrat de `0x822F8848`, effet XMA,
  source EDRAM non nulle, pixels, audio et mission.

Prochain test ciblé : joindre le corps de `0x822F8848` à l'objet/vtable
appelant et capturer une fenêtre courte autour de sa première activation,
puis reprendre la qualification EDRAM non nulle. Ne pas appliquer le patch
Xenia au runtime PAL et ne pas utiliser `ptrace` comme correctif de scheduling.


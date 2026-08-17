# Cycle 1646 — couverture complète du hand-off événementiel PAL

## Résultat

Le filtre `AC6_DEMO_WATCH_EVENT_HANDOFF_FOCUSED=1` réduit la sortie aux
entrées signal/wait, publications de réveil, set/clear/pulse et resume. Il
augmente uniquement la limite de journalisation à 32 768 records ; il ne
modifie aucun état guest ni règle de réveil.

Deux processus codegen-ON frais ont été rejoués sur le store PAL démo, neutral
et START au tick 252, jusqu'au tick 600.

| mesure | neutral | START |
|---|---:|---:|
| records | 13 785 | 13 785 |
| intervalle couvert | 0–599 | 0–599 |
| SHA stderr | `061757ba…14cfb6` | `061757ba…14cfb6` |
| ticks terminés | 600 | 600 |
| PRESENT | 463 | 463 |
| frontend / mission / terminal | non / non / non | non / non / non |

Les lignes parsées sont byte-identiques entre les deux routes. Les sous-arbres
`outcome`, `milestones`, `graphics` et `scheduler` le sont également.

## Hand-offs observés

- `0xE0000048 → 0xE000004C` : 1 402 records `signal_wait`, threads `[1]`,
  ticks 0–599, LR `0x821A69CC` ;
- `0xE0000054 → 0xE0000058` : 1 415 records, threads `[1,14]`, ticks 0–599,
  LR `0x821A69CC` ;
- `set_enter/set_exit` : 3 210 paires ; `clear` : 55 ; `pulse_enter/pulse_exit`
  : 2 paires aux ticks 112 et 264 ; `resume_thread` : 11, ticks 0–106 ;
- `event_wake` : 3 264 ; `signal_wait_block/resume` : 706/704 ;
  `wait_single_block/resume` : 612/602.

Ces valeurs sont des observations du bridge PAL synthétique (`0xE000…`). Les
handles Xenia `0xF8000088/0xF800008C` et sa pile `pthread_cond_wait` restent
`xenia-generic`; aucune égalité d'handles n'est affirmée.

## Qualification

- `demo-qualified` : couverture 0–599, A/B byte-identique, opérations
  set/clear/pulse/resume tracées et comportement inchangé ;
- `demo-observed` : comptes et couples `E000…` du runtime instrumenté ;
- `xenia-generic` : seulement les handles et backtraces de l'archive Xenia ;
- `unknown` : jointure sémantique Xenia↔PAL, PC distinct du LR d'import et
  relation avec un frontend.

Le readback reste noir et aucun frontend guest-owned n'est atteint. Aucune
screencap n'est donc produite ou promue.

Le reçu durable est
[`ac6-demo-event-handoff-focused-v1.json`](../analysis/demo/ac6-demo-event-handoff-focused-v1.json).

## Prochain checkpoint

Joindre les 11 `resume_thread` et les pulses des ticks 112/264 aux writers
guest exacts, puis reprendre le payload de queue. Ne pas translater les
handles Xenia dans le runtime PAL et ne pas promouvoir START, readback ou
screencap avant une transition guest persistante et une surface non noire.

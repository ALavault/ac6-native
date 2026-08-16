# Cycle 1760 — event handoff démo PAL, A/B à 5600 ticks

## Résultat

STOP : la frontière d'observation reste le transport d'événements kernel/XAM.
Les deux probes Vulkan qualifiés (`neutral` et `buttons=16` au tick 252)
finissent par le terminal contrôlé `max_ticks` (rc 4) à 5600 ticks, avec 5 463
PRESENT et 23 threads bloqués / 0 runnable. Aucun frontend, mission, terminal,
divergence graphique ou readback n'est observé. L'accès guest post-reprise
n'est pas observé dans cette capsule bornée; cela ne signifie pas qu'aucun
accès n'a eu lieu jusqu'au tick 5600. `supported=false`.

L'identité est `ac6-demo-xbox360-pal`, `Default.xex`, SHA-256
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`,
projet Ghidra `ace-combat-6-demo`, basefile
`b98a9ac1…581aa14218`. Le binaire codegen-ON historique qualifié a SHA-256
`e2b3baf1…89719d7847`; manifest codegen `9f1fffb0…64a935bbfa`, manifeste
Ghidra `576fa31e…aeb2810086`, config frontière `4a87dd8c…15c0e54506`.
Les sources retail et les projets Ghidra historiques sont refusés.

## Handoff `0xE000004C`

Les stderr sont byte-identiques, SHA-256 `f48e4568…4002da5c7f27`. La trace
focused est plafonnée à 32 768 records et sa dernière couverture est le tick
1212, alors que le run va jusqu'au tick 5600. Par route, elle contient 2 176
`set_enter` / 2 176
`event_wake` / 2 176 `set_exit`, avec 963 wakes vers waiter 1 et 962
`signal_wait_resume` du thread 1. Ces 962 chaînes complètes sont
`thread 12 set → wake(waiter 1) → thread 1 resume`; les LR sont
`0x821A6AC4` (set) et `0x821A69CC` (wait/resume). La dernière wake enregistrée
est tronquée avant sa reprise suivante par la borne de log. Aucun
`wait_single_block` ni `wait_single_resume` ne vise `0xE000004C` dans cette
fenêtre (compte 0/0).

La sonde établit donc que le réveil est livré, mais elle n'observe pas dans
cette capsule le critère GO restant : un accès guest post-reprise et une
divergence rendu/readback. Le booléen `post_resume_guest_access_observed=false`
est borné à cette observation et ne conclut pas à l'absence d'accès pendant
le reste du run à 5600 ticks. Les deux rapports ne diffèrent que par les deux compteurs
`RtlEnterCriticalSection`/`RtlLeaveCriticalSection` déjà vus à l'entrée;
milestones, ordonnanceur et graphiques sont inchangés.

## Exécution et artefacts

Commande essentielle (une fois par route, avec l'entrée seulement pour
`buttons=16`) :

```text
SDL_AUDIODRIVER=dummy AC6_DEMO_EXPERIMENTAL_XMA_CREATE=1 AC6_DEMO_EXPERIMENTAL_XMA_KICK=1 AC6_DEMO_WATCH_EVENT_HANDOFF=1 AC6_DEMO_WATCH_EVENT_HANDOFF_FOCUSED=1 xvfb-run -a ac6-demo-recomp probe --store <fresh-store> --until frontend --max-ticks 5600 --trace <trace> --report <report> --backend vulkan [--input-at 252,16,0,0,0,0,0,0,1]
```

Les traces et rapports reproduisent exactement le reçu cycle 1756 : neutral
report/trace `99dba4f5…12396533` / `ba3871e2…e1a4df562`; buttons=16
`90d27b58…303ee468` / `30bb9fb6…ba43bd946`. Les sorties temporaires bornées
sont sous `/fastdata/lavaulta/tmp/ac6-cycle1760-event-handoff.IveLQv`.

Validations : SHA-256 XEX/basefile/codegen/manifestes, rc attendu 4 sur les
deux routes, `cmp` stderr PASS et diff JSON ciblé (seuls les deux compteurs
runtime et le hash de trace diffèrent). Aucun runtime, source ou output généré
n'a été modifié.

## Risques résiduels

La capture focused est bornée avant la fin du run; le STOP est une frontière
d'observation kernel/XAM event transport et ne permet pas d'inférer la
sémantique complète du contrat derrière la reprise. XMA create/kick est
une garde opt-in, pas une implémentation XMA qualifiée. Frontend, mission,
terminal et pixels guest-owned restent inconnus.

# Cycle 1648 — writers guest exacts des handles PAL

## Résultat

Deux processus codegen-ON frais ont été exécutés depuis le store PAL démo
`.build/ac6-demo-store-test-3`, neutral puis START `0x0010` au tick 252, avec
le hook opt-in `AC6_DEMO_WATCH_EVENT_HANDLE_WRITERS=1`. Le hook observe les
`AC6_PPC_STORE_U32` dont la valeur appartient à un objet bridge connu ou à un
thread guest ; il ne modifie jamais la mémoire ni le scheduler.

| mesure | neutral | START |
|---|---:|---:|
| ticks terminés | 600 | 600 |
| PRESENT | 463 | 463 |
| lignes writers | 163 | 163 |
| SHA lignes writers | `735d6faf…e3411724` | `735d6faf…e3411724` |
| SHA stderr complet | `5fa5887b…2822a8566` | `5fa5887b…2822a8566` |
| frontend / mission / terminal | non / non / non | non / non / non |

Le reçu durable est
[`ac6-demo-event-handle-writer-probe-v1.json`](../analysis/demo/ac6-demo-event-handle-writer-probe-v1.json).

## Jointure exacte

La capture dynamique fournit adresse guest, valeur, tick, thread, LR courant,
fonction générée et ligne source. Les lignes générées ont été remappées
séquentiellement aux instructions Xenon puis vérifiées dans le basefile PAL
`b98a9ac1…4218`. Elle produit 25 sites distincts. Les trois sites de
construction de la table initiale sont :

| fonction | PC guest | mot PAL | LR runtime | valeurs/tick |
|---|---|---|---|---|
| `0x821A1E38` | `0x821A1EAC` | `939EFFF0` | `0x821A1EA4` | `E1000000..000C`, t0 |
| `0x821A1E38` | `0x821A1ED0` | `917E0000` | `0x821A1EBC` | `E0000000..0018`, t0 |
| `0x821A1E38` | `0x821A1EE0` | `917E0010` | `0x821A1ED8` | `E0000004..001C`, t0 |

Le couple de handles transmis aux chemins `signal/wait` est écrit par
`0x822EED70` aux PCs `0x822EEDA8` (`917F0000`) et `0x822EEDB4`
(`907F0004`). Les écritures répétées de structures aux ticks 40, 106, 117,
223 et 257 proviennent de `0x8219A060`, PCs `0x8219A4B8` (`907F5960`) et
`0x8219A4F4` (`913F595C`). Le tableau complet des 25 sites, adresses guest,
plages de valeurs et LR est dans le JSON.

## Qualification et limites

- `demo-qualified` : PCs et mots PAL exacts, 25 sites, couverture A/B et
  digest des 163 lignes identiques ;
- `demo-observed` : valeurs `E000…/E100…`, ticks, threads et adresses écrites ;
- `xenia-generic` : uniquement les handles `F800…` et la pile POSIX de
  l’archive Xenia ;
- `unknown` : type d’objet, consumer causal et relation sémantique PAL↔Xenia.

Le readback reste noir, aucun frontend guest-owned n’est atteint et aucune
screencap n’est donc produite ou promue. Le prochain test doit suivre un seul
des sites prioritaires jusqu’à son consumer, sans injecter d’état visuel.

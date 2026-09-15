# r591 — Cause racine du gel MissionTitle : le décodeur PM4 rejette l'IB 0x12cc47c0 comme « tronqué » en boucle (1,48 M), le curseur ring n'avance plus, le fence de flip se fige

## Qualification

- **Ghidra project** : `ghidra-projects/ace-combat-6`, `default.xex` (retail
  `ntsc-uj`). Résolution hôte→guest via `exec_base`+`addr2line`.
- **Oracle** : non. Aucun budget N3.
- **Preuve runtime** : run bootée jusqu'à MissionTitle,
  `AC6_NATIVE_VD_TRACE=1` (+ BOOT_PHASE_TRACE, THREAD_SAMPLE des cycles
  précédents). GPU saturé.

## La chaîne complète (du game loop à la cause racine)

```
Function_821D7D90  (main game loop, while(true))
  -> sub_821F03B0 -> sub_821E61A8  : frame gate, attend le fence de flip
        completion slot = *(producer_obj+10896) = 0x16520000, veut >= requested
  -> le fence est avancé par advance_flip_completion_locked() sur present OK,
     ET par les paquets fence-write de l'IB ring
  -> l'IB ring 0x12cc47c0 est REJETÉ par le décodeur PM4 (« tronqué »)
  -> le curseur ring n'avance pas -> les paquets fence-write (0x16520006,
     0x16520002) ne s'exécutent jamais -> le fence reste figé -> le gate bloque.
```

## Cause racine (dominante, mesurée)

Tally à MissionTitle (VD_TRACE) :
- **`vd drain rejected decode_ok=0 code=2 ... PM4 packet payload is truncated
  (IB 0x12cc47c0)` : 1 482 642 occurrences** — le drain retente sans fin de
  décoder l'IB `0x12cc47c0` (254 mots), échoue « tronqué » à l'offset 254, drop
  (curseur inchangé), retente. **C'est le livelock.**
- `active shader is not pinned` : 18 (mineur, séparé — voir plus bas).
- `vd drain rejected decode_ok=1` (fallback backend après unpinned) : 18.
- `vd swap presented` : 1707 — **les presents RÉUSSISSENT** (present_count
  monte à 1707 alors que `updates` est figé à 1697).

Donc le décodeur PM4 (`Pm4Decoder::decode_one`, `native_xenos.cpp:130`,
`require_words`) juge un paquet de l'IB `0x12cc47c0` tronqué : sa longueur
déclarée dépasse les mots restants. La queue de l'IB contient précisément les
paquets fence-write (`c0025800 00000003 16520006 ...` et `... 16520002`) qui
avanceraient le fence attendu par le game loop. Le rejet en boucle les prive
d'exécution.

## Corrections de mes hypothèses intermédiaires (preuve runtime)

- **« present échoue »** (r591 provisoire) : **FAUX**. `present_to_offscreen`
  réussit (1707 presents, 0 échec) ; `advance_flip_completion_locked` est bien
  appelé sur le chemin VdSwap direct. Le fence de present avance ; c'est le fence
  écrit **par les paquets ring** (via l'IB rejeté) qui se fige.
- **« nœud loader manquant / désarme f24 / DAT_8293ba10 / graphe d'events »**
  (r584-r590) : tous **en aval**. Le vrai blocage est le drain ring bloqué sur
  l'IB tronqué.
- Le nœud `0xb362294c` **existe** (r589). « register the node » réfuté.

## Le facteur mineur : shaders non pinnés (18)

18 fois : `pinned draw: active shader is not pinned: no pinned variant matches
this draw state` → `pinned_->execute_frame` rejette, fallback
`backend_->submit` rejette aussi (`decode_ok=1 backend commands=81`), batch
droppé. C'est la couverture shaders insuffisante (plan Couche 4), réel mais
**marginal** (18 vs 1,48 M) devant la troncature d'IB. Les deux privent le ring
d'avancer, mais la troncature domine massivement le gel.

## Non établi (dit clairement)

- **Pourquoi l'IB `0x12cc47c0` (254 mots) est jugé tronqué à l'offset 254** : un
  paquet en fin d'IB déclare une longueur dépassant les mots restants. Reste à
  déterminer si (a) le champ taille de l'`INDIRECT_BUFFER` (`0x000000fe`=254) est
  correct et un paquet type3 en fin déborde (bug guest improbable), (b) le
  décodeur mésestime la longueur d'un opcode de fin (ex. `0x3b`, `0x58`), ou (c)
  l'IB réel dépasse 254 mots (la « trail » `c0025800...16520006` est censée en
  faire partie) et la taille est mal lue. Le dump `vd ib diag`
  (fetch_head/tail/trail) est capturé pour cette analyse.
- La relation exacte entre le fence de present (avance) et le fence ring-écrit
  (figé) — deux slots ou le même consommé différemment.

## Prochaine barrière (précise, GPU-libre pour l'analyse)

Analyser `Pm4Decoder::decode_one` + le chemin IB du drain
(`native_guest_vd.cpp` drain_locked / VdBridge IB fetch) contre le dump
`vd ib diag addr=0x12cc47c0 ... fetch_tail=... trail=c0025800 00000003 16520006
...` : décoder les paquets de fin de l'IB à la main pour trouver lequel est
jugé tronqué et pourquoi (longueur d'opcode vs taille d'IB). Le fix est côté
décodeur/drain (parser correctement, ou avancer le curseur au-delà d'un IB
non décodable pour ne pas prendre le fence en otage). Validation = un run
(GPU-borné) où `updates` avance au-delà de MissionTitle.

## Décisions prises

1. **Épingler la cause racine sur preuve** (tally VD_TRACE : troncature IB
   1,48 M dominante) plutôt que sur hypothèse ; corriger « present échoue » et
   toute la piste loader/event comme en aval.
2. **Ne pas conclure le fix** : le paquet exact jugé tronqué et le correctif du
   décodeur restent à établir par analyse du dump `vd ib diag` (nommée comme
   prochaine étape), pas devinés.
3. Aucun code de fix ce cycle ; hooks/diagnostics conservés en scaffolding local.

# Checkpoint 4l — fenêtre de replay native sans progression de script

Date : 2026-08-12

Le replay de la fenêtre `mission01-controlled-sortie` a été relancé après
`bf78a8465a8263cfb190a06b2a5412c204a04c68`. Le mode `--trace` désactive
explicitement l'avancement du script de mission : la route oracle est une
fenêtre de vol de 3 600 échantillons, et non les six étapes de scénario
précédemment consommées par le replay natif.

Artefacts scellés :

| artefact | valeur |
|---|---|
| replay | `/tmp/ac6-mission01-oracle-a.replay` — SHA-256 `a04dccbfb8f032e6747fb96a643afd1b4824ebd4458f72de10471679ca796d07` |
| cache | `/tmp/ac6-retail-v2-smoke` — index SHA-256 `cfca517e3f843169ca01fc52700472e66b86365621a922fc27a64a21ab713f85` |
| binaire natif | `reconstruction/ace-combat-6/build/ac6-native` — SHA-256 `95db5bb4c2b847fec347b03f4ce91d852b625210b18c5c69cf195f7c6929508d` |
| trace brute native | `/tmp/ac6-native-trace-window-v2.7YvsP1/native.trace.jsonl` — SHA-256 `1afc905e537c9df7be48b3c24d054dc080f526b192242a9abed2cdfd2a69775e` |
| trace v2 native | `/tmp/ac6-native-current.execution-trace.v2.json` — SHA-256 `d11f1757cb74bf8cf4ca73b25d87579dd851d74ce23ef6a55accee698c77c1dd` |
| trace v2 oracle | `/tmp/ac6-oracle-current.execution-trace.v2.json` — SHA-256 `56f807150428bfbfba360ba6c00ac2a9c1eddfabe47b4dc6d57a215102053f71` |

Le binaire rapporte `frames=7200`, `final_tick=7200`,
`script_ended=false`, `trace_samples=3600` et `trace_events=18000`.
Le replay reste déterministe (`semantic_hash=0x6b482629e4fd6d6c`).

Comparaison avec `tools/compare_ac6_execution_traces.py` :

* le domaine `controller_input` est identique sur les 3 600 événements
  (`/tmp/ac6-window-input-compare.json`, SHA-256
  `191d385cbb2192bf0ebdc157112e808f4bc8edeec6adc0b1063c625758be4612`) ;
* la comparaison des cinq domaines s'arrête au premier écart à
  `sequence=1`, `tick=1`, domaine `simulation_snapshot`, chemin
  `events[1].payload.active_units` : champ absent dans l'oracle, `230` dans
  le natif (`/tmp/ac6-window-full-compare.json`, SHA-256
  `46c5f6916da85ddfdf1cb3a544119e2d4660488fbfbaa6434f6550b8a68029ab`).

Cet écart est désormais un écart de producteur/contrat de snapshot (les
payloads mission et soumission graphique ont également des schémas différents),
pas une preuve de divergence de commandes. Il ne ferme donc pas encore le
triangle oracle↔natif ni l'éligibilité JV/JP. Le rapport historique
`checkpoint-3-oracle-native-first-divergence.json` est conservé comme trace de
la fausse fin de script ; il n'est pas réécrit.

Validation ciblée : build `ac6-native` et test `ac6-retail-session` passants ;
les tests Python du traceur/comparateur passent. Le renderer de production
reste le chemin CPU de compatibilité ; l'adaptateur Vulkan store-backed de
`checkpoint-4j` reste expérimental (`jv_eligible=false`).

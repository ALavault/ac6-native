# Cycle 971 — boucle résultat → débriefing → campagne

## Contrat

La mission native pouvait atteindre `Complete` ou `Aborted`, mais le frontend
ne matérialisait pas le débriefing ni le retour campagne. Le nouveau contrat
est :

```text
Mission + MissionOutcome::Success/Failure
  -> Debrief
  -> NewGame (sélection effacée)
```

`enter_debrief` vérifie l’identité de mission et, lorsqu’une campagne est
attachée, l’état de progression attendu. Une mission encore `InProgress`, une
mission différente ou une progression incohérente sont refusées.

## Validation

Le test runtime couvre :

- deux objectifs complétés, campagne `Completed`, débrief succès ;
- destruction du joueur, débrief échec ;
- retour campagne dans les deux cas, avec sélection et débrief nettoyés.

```text
cmake --build build -j2
DISPLAY=:100 SDL_AUDIODRIVER=dummy ctest --test-dir build --output-on-failure
100% tests passed, 0 tests failed out of 5
sdl_vulkan_surface=1 extensions=2 queue_family=0 swapchain_images=3
campaign_catalog=pass missions=15 qualified=1 partial=1 unqualified=13
code_inventory=pass roots=7 native_covered=7 retail_partial=6 retail_unknown=1 entries=16
```

Ce contrat est natif et générique ; il ne revendique pas encore une capture
retail interactive.

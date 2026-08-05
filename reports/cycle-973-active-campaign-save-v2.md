# Cycle 973 — persistance d’une campagne active

## Défaut fermé

Le snapshot précédent n’enregistrait que les missions terminées. Une session
avec un checkpoint `Active` perdait donc son état campagne au rechargement et
ne pouvait plus satisfaire la garde `MissionExecution::launch` qui exige
`CampaignMissionState::Active`.

## Format

`CampaignSaveSnapshot` version 2 encode par record :

```text
mission_id, objective_mask, state, aircraft_id, weapon_id, capability_data_valid
```

`AC6SESS` version 5 transporte ces records. Les codecs v1 campagne et v1–v4
session restent lisibles ; leurs records sont interprétés comme des missions
terminées, conformément à leur format historique.

## Validation

Le test couvre le snapshot en mémoire et le fichier session : une mission
active avec loadout `{7,8,true}` et masque objectif `1` est restaurée comme
`Active`, avec le même loadout et masque.

```text
cmake --build build -j2
DISPLAY=:103 SDL_AUDIODRIVER=dummy xdpyinfo >/dev/null
DISPLAY=:103 SDL_AUDIODRIVER=dummy ctest --test-dir build --output-on-failure
100% tests passed, 0 tests failed out of 5
sdl_vulkan_surface=1 extensions=2 queue_family=0 swapchain_images=3
campaign_catalog=pass missions=15 qualified=1 partial=1 unqualified=13
code_inventory=pass roots=7 native_covered=7 retail_partial=6 retail_unknown=1 entries=16
```

La persistance est générique et ne prétend pas reproduire le format retail
Xbox 360.

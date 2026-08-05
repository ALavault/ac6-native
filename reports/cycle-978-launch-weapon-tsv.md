# Cycle 978 — launch TSV avec armes

## Contrat de données

Le launch manifest accepte :

```text
mission_id<TAB>player_entity<TAB>unit_id:owner:asset,...
mission_id<TAB>player_entity<TAB>unit_id:owner:asset,...<TAB>id:damage:projectile_speed:cooldown:max_range,...
```

La seconde forme est rétrocompatible avec la première. Les flottants doivent
être finis, les paramètres satisfaire `WeaponDefinition::valid`, et les IDs
d’armes être uniques.

## Preuve

Le test charge le launch TSV avec l’arme `7:60:20:0.25:100`, vérifie sa
présence, puis exerce le chemin mission verrouillage → tir → collision →
dégâts.

```text
cmake --build build -j2
DISPLAY=:109 SDL_AUDIODRIVER=dummy xdpyinfo >/dev/null
DISPLAY=:109 SDL_AUDIODRIVER=dummy ctest --test-dir build --output-on-failure
100% tests passed, 0 tests failed out of 5
sdl_vulkan_surface=1 extensions=2 queue_family=0 swapchain_images=3
campaign_manifest=pass qualified=1
campaign_catalog=pass missions=15 qualified=1 partial=1 unqualified=13
code_inventory=pass roots=7 native_covered=7 retail_partial=6 retail_unknown=1 entries=16
```

Les valeurs restent des paramètres natifs tant que les templates retail ne
sont pas qualifiés.

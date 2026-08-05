# Cycle 974 — snapshots campagne fail-closed

## Invariant

Un record sauvegardé doit représenter une mission `Briefing`, `Active`,
`Completed` ou `Failed`, et son masque doit être contenu dans les objectifs de
la définition. Toute violation est rejetée avant la remise à zéro ou la
publication de l’état courant.

## Correction

`CampaignProgression::restore` valide l’état, le masque borné et le loadout
actif avant d’appliquer les records. `CampaignSaveStore` et `AC6SESS`
appliquent la même borne d’état au niveau conteneur ; les codecs historiques
restent lisibles.

## Preuve

Le test restaure une campagne active, soumet un masque hors bornes puis un
état `Available`, et vérifie dans les deux cas que la campagne reste active.

```text
cmake --build build -j2
DISPLAY=:104 SDL_AUDIODRIVER=dummy xdpyinfo >/dev/null
DISPLAY=:104 SDL_AUDIODRIVER=dummy ctest --test-dir build --output-on-failure
100% tests passed, 0 tests failed out of 5
sdl_vulkan_surface=1 extensions=2 queue_family=0 swapchain_images=3
campaign_catalog=pass missions=15 qualified=1 partial=1 unqualified=13
code_inventory=pass roots=7 native_covered=7 retail_partial=6 retail_unknown=1 entries=16
```

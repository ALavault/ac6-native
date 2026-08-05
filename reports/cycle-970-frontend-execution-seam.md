# Cycle 970 — raccord frontend → MissionExecution

## Contrat

Le frontend validait auparavant le parcours jusqu’à l’état `Mission`, mais le
consommateur devait relancer manuellement le launch manifest. Cette séparation
permettait à une exécution de diverger de la mission sélectionnée.

`FrontendController::launch_selected` est une frontière générique :

```text
FrontendState::Mission
  -> selected_mission
  -> MissionCatalog
  -> MissionLaunchDatabase
  -> MissionExecution::launch
```

La méthode ne contient aucun `mission_id` codé en dur, refuse les manifestes
incomplets et refuse une exécution déjà lancée.

## Validation

Le test runtime configure la campagne, traverse Title → New Game → Briefing →
Hangar → Loading → Mission, applique un loadout valide, lance l’exécution via
la nouvelle frontière et vérifie `ScenarioState::Gameplay`. Une seconde
tentative échoue.

```text
cmake --build build -j2
DISPLAY=:99 SDL_AUDIODRIVER=dummy ctest --test-dir build --output-on-failure
100% tests passed, 0 tests failed out of 5
sdl_vulkan_surface=1 extensions=2 queue_family=0 swapchain_images=3
campaign_catalog=pass missions=15 qualified=1 partial=1 unqualified=13
code_inventory=pass roots=7 native_covered=7 retail_partial=6 retail_unknown=1 entries=16
```

Ce raccord qualifie l’orchestration native et ne constitue pas une preuve
interactive retail Xenia.

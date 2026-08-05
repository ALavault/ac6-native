# Cycle 982 — sauvegarde disque d’une mission active

## Contrat

Une sauvegarde de mission active doit conserver ensemble la progression
campagne, l’état HSM/objectifs, le joueur et les unités combat. Après reload,
une nouvelle exécution doit pouvoir reprendre le vol et utiliser le même
armement, sans réécrire de force l’état invité.

## Correction d’intégrité

`MissionExecution::restore_checkpoint` exige maintenant que le joueur déclaré
par le snapshot soit présent dans `combat_units`. Le même invariant est
appliqué par `SessionSaveStore` avant insertion ou lecture du fichier. Un
checkpoint qui référence une entité absente est refusé avant mutation.

## Preuve end-to-end

Le test construit une campagne Mission 1 `Active`, applique le loadout,
complète l’objectif 1 sur 2, avance le vol, puis sauvegarde le checkpoint et
la campagne dans `AC6SESS`. Le fichier est relu dans un nouveau store ; une
nouvelle campagne et une nouvelle exécution sont lancées depuis les données
rechargées. L’objectif reste `Complete`, le masque campagne vaut `1`, et une
salve de l’arme `7` touche encore la cible. Une variante dont le joueur est
remplacé par l’entité `9999` est refusée par le runtime et le store.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j2
SDL_AUDIODRIVER=dummy ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
100% tests passed, 0 tests failed out of 5
sdl_vulkan_surface=1 extensions=2 queue_family=0 swapchain_images=3
campaign_manifest=pass qualified=1
campaign_catalog=pass missions=15 qualified=1 partial=1 unqualified=13
code_inventory=pass roots=7 native_covered=7 retail_partial=6 retail_unknown=1 entries=16
```

Frontière : persistance native générique `AC6SESS`, sans modification du C++
généré. Les paramètres retail exacts de Mission 1 et la parité interactive
stock restent à qualifier.

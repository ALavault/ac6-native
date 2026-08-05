# Cycle 983 — identité des ressources dans les checkpoints

## Contrat

Une reprise ne doit pas réutiliser silencieusement un payload différent de
celui qui a produit le checkpoint. L’identité persistée comprend l’ID
d’asset, le chemin relatif et le hash du manifeste, dans un ordre déterministe.

## Implémentation

`MissionExecution::save_checkpoint` collecte les `AssetRecord` de tous les
assets de la définition dans un candidat local, les trie par ID et ne publie
le checkpoint qu’après résolution complète. `restore_checkpoint` valide les
entrées triées puis compare chaque record au `MissionAssetDatabase` courant.

`SessionSaveStore` écrit désormais `AC6SESS` version 6 et encode les chemins et
hashes bornés. La lecture des versions 1 à 5 reste supportée ; les anciens
checkpoints sans identité sont acceptés uniquement par ce chemin de
compatibilité, sans fabriquer de hash.

## Preuve

Le test Mission 1 vérifie le round-trip disque avec les identités capturées,
puis modifie le chemin d’un checkpoint rechargé et vérifie le refus avant
restauration. Le test session relit aussi un fichier v5 sans checkpoint, ce qui
garde la compatibilité historique.

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

Frontière : les versions historiques ne peuvent pas être rétroactivement
complétées en hashes ; elles restent explicitement sur leur contrat legacy.
La qualification retail interactive des missions 2–15 demeure ouverte.

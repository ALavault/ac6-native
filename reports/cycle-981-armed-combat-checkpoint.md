# Cycle 981 — checkpoint de combat armé

## Contrat

La reprise d’une mission active doit conserver les capacités de combat
publiées au lancement : templates d’armes, verrouillage et possibilité de
tirer après restauration. Les unités du checkpoint restent la représentation
persistée du monde ; `CombatWorld::restore_units` ne doit pas effacer les
templates d’armes de l’exécution lancée.

## Correction runtime

La validation a d’abord trouvé un défaut indépendant mais bloquant : avec
quatre appels `tick(250 ms)`, `MissionRuntime` plafonnait à 8 steps et gardait
une dette de simulation supérieure à une frame, alors que `RuntimeSnapshot`
et `SaveStore` exigent un accumulateur inférieur à `1/60 s`. Le plafond passe
à 16 steps, soit assez pour les 15 steps de la fenêtre maximale de 250 ms ;
la soustraction est aussi bornée à zéro pour éviter une valeur flottante
négative.

## Preuve

Le test de produit lance deux unités et l’arme `7`, tire une première fois,
attend la résolution du projectile, sauvegarde, inflige 30 dégâts hors tir,
puis restaure. Il vérifie la santé restaurée à `40`, verrouille la cible,
retire une seconde salve et vérifie une santé finale à `0`.

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

Frontière : correction runtime native, sans modification du C++ généré.
Les paramètres retail d’armes et la parité complète de la Mission 1 restent
à qualifier depuis les assets et l’oracle stock.

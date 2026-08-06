# Cycle 850 — pompe d'événements SDL3

Date: 2026-08-04

## Résultat

`SdlEventPump` possède maintenant l'initialisation/fermeture du sous-système
SDL events (avec gamepad optionnel), dépile `SDL_PollEvent`, transmet axes et
boutons à `SdlInputAdapter` et convertit `SDL_EVENT_QUIT` en drapeau de sortie.
Une pompe non initialisée refuse de consommer des événements.

La fixture initialise SDL sous `SDL_VIDEODRIVER=dummy`, injecte un événement
QUIT via `SDL_PushEvent`, vérifie le drapeau puis ferme explicitement le
sous-système. Le cœur reste indépendant de SDL3.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j2                 OK
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure
1/1 test passed
ac6-native (mêmes variables)                                      exit 0
```

## Limite

La pompe n'ouvre pas encore de fenêtre, ne gère pas les périphériques ajoutés
ou retirés et n'est pas appelée par la boucle principale `ac6-native`. Le
raccord SDL3→WorldFrame est donc testable mais pas encore interactif.

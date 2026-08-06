# Cycle 924 — cycle de vie des périphériques SDL3

`SdlEventPump` traite explicitement les événements gamepad ajoutés/retirés.
Un retrait appelle `SdlInputAdapter::reset`, purge les touches maintenues,
axes et boutons, puis continue la boucle sans générer d’événement gameplay
spurious. L’absence de backend gamepad reste non fatale.

Le test pousse un événement `SDL_EVENT_GAMEPAD_REMOVED` et vérifie la remise à
zéro complète.

Validations : CTest normal 3/3, ASan/UBSan 3/3, package audit pass.

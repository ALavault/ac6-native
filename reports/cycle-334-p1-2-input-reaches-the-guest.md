# Cycle 334 — P1.2 : l'entrée atteint l'invité ; son effet reste non attribué

## 0. Résultat en une ligne

**Acquis** : une touche pressée atteint réellement l'invité, mesuré.
**Non acquis** : que ce soit *elle* qui change l'état présenté.

## 1. Pourquoi le test du cycle 314 ne pouvait pas marcher

Il envoyait `Return`, `space`, `KP_Enter` et concluait « l'entrée est morte ».
Deux raisons indépendantes le condamnaient :

1. **Les touches.** Le pilote MnK lie `Start` à **`Escape`** et `A` à `Space` ;
   `Return` et `KP_Enter` ne sont liés à **rien**. Deux des trois touches
   envoyées ne pouvaient rien produire quel que soit l'état de l'invité.
2. **Le pilote est désactivé par défaut.**
   `REXCVAR_DEFINE_BOOL(mnk_mode, false, ...)`, et `IsEnabled()` garde
   `GetState`, qui rend alors `X_ERROR_DEVICE_NOT_CONNECTED`. Sans
   `--mnk_mode=true`, **l'invité ne voit aucune manette**.

## 2. Pourquoi rejouer ce test tel quel serait pire aujourd'hui

Depuis l'ouverture de la porte P0, **le contenu présenté est animé**. Deux
exécutions sans aucune entrée diffèrent déjà fortement. Mesuré, protocole
A/B/T à horaires identiques (A et B sans entrée, T avec entrée après l'image 5) :

```
plancher de bruit, max |A-B| après la pression : 116,190
effet mesuré,     moyenne |A-T| après          :  36,294
rapport                                        :   0,31x
```

L'écart entre deux exécutions **intactes** est trois fois plus grand que
l'écart attribuable à l'entrée. **Une comparaison de pixels ne peut pas
trancher cette question** — ni dans un sens ni dans l'autre. Le verdict
« aucun effet » qu'elle rend est sans valeur, et c'est exactement le piège où
le cycle 314 est tombé, à ceci près qu'il l'ignorait.

## 3. L'observable qui tranche : l'état, pas les pixels

Sonde ajoutée dans `MnkInputDriver::GetState`, qui déclare les trois conditions
décidant qu'une touche puisse atteindre l'invité : pilote actif, fenêtre
focalisée, nombre de touches enfoncées.

Avec `--mnk_mode=true`, sur serveur X sans gestionnaire de fenêtres :

```
[mnk-probe] active=true has_focus=true keys_down=0     <- xdotool key
...
[mnk-probe] active=true has_focus=true keys_down=1     <- xdotool keydown/keyup
```

Deux faits :

- `active=true has_focus=true` : le pilote est prêt et l'invité **interroge**
  `GetState` en continu.
- `keys_down=0` avec `xdotool key`, `keys_down=1` avec `keydown`/`keyup`
  maintenu. `xdotool key` presse et relâche en ~12 ms ; à la fréquence
  d'interrogation de l'invité, l'appui **tombe entre deux lectures**.
  85 lectures à `keys_down=1` pendant un maintien de 1,5 s d'`Escape`,
  153 après un maintien de `Space`.

**L'entrée atteint l'invité.** C'est la moitié nécessaire de P1.2, et elle est
acquise. Elle n'était acquise dans aucun cycle précédent.

Troisième raison, donc, pour laquelle le cycle 314 ne pouvait pas réussir :
même avec les bonnes touches et le pilote activé, une frappe instantanée est
invisible à l'invité.

## 4. Ce qui n'est pas établi

La capture après entrée montre l'écran-titre « ACE COMBAT / Fires of
Liberation » à 59,57 im/s et 139 660 dessins hôte, là où la capture précédente
montrait un survol de ville à 29,76 im/s. C'est un **changement d'état réel**.

**Il n'est pas attribué à l'entrée.** La séquence d'attrait avance seule :
intro, puis carte de titre. Sans exécution témoin sur la même chronologie, ce
changement est tout aussi bien une progression normale. L'affirmer serait
refaire l'erreur du cycle 314 en sens inverse — conclure d'une corrélation
temporelle unique.

## 5. Front suivant

Attribuer, avec un observable insensible au bruit d'animation :

1. Journaliser dans `XamInputGetState` le masque de boutons **rendu à
   l'invité** et le compteur de paquets : prouve que l'invité lit une valeur
   non nulle, pas seulement que le pilote la détient.
2. Choisir un état à transition **franche et persistante** plutôt que l'écran
   de titre animé — un menu qui reste ouvert — et le tester en A/B avec maintien
   de touche, sur exécutions témoin appariées.
3. Rendre le maintien explicite dans tout futur outil d'entrée : `keydown`,
   pause d'au moins 200 ms, `keyup`. Jamais `xdotool key`.

## 6. Règles ajoutées

1. **Une entrée synthétique doit être maintenue.** `xdotool key` presse et
   relâche trop vite pour un invité interrogé à la trame ; l'appui disparaît
   entre deux lectures et le test rapporte « aucune entrée » alors que le
   chemin est sain.
2. **Vérifier qu'un sous-système est activé avant de conclure qu'il est cassé.**
   `mnk_mode` est faux par défaut ; trois cycles auraient pu être évités.
3. **Quand le témoin bruite plus fort que l'effet, le test ne rend aucun
   verdict — il faut changer d'observable.** « Aucun effet mesurable » ne veut
   pas dire « aucun effet ».

`recompiler-generated` n'est pas `verified`. Ni jouable, ni parité retail.

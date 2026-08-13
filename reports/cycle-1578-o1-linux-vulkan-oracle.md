# Cycle 1578 — O1 oracle NTSC-U/J Linux/Vulkan exécutable

Date : 2026-08-13.

## Résultat

L'oracle comportemental NTSC-U/J `AC6_recomp@ab90b547…` construit et démarre
désormais nativement sous Linux avec Vulkan seul. La pile versionnée contient
le patch poll-exact v4 puis `linux-vulkan-minimal-v1.patch`; aucun fichier
généré ni octet retail n'est versionné. Le profil minimal exclut les sources
D3D12/Win32 et toutes les améliorations AC6, conserve résolution 1× et cadence
retail, désactive compilation shader asynchrone et workers de pipelines, et
utilise `SDL_AUDIODRIVER=dummy` sous Xvfb.

Trois défauts POSIX bornés ont été corrigés avec gardes :

- un signal d'événement auto-reset réservé à un waiter existant ne peut plus
  être volé par le waiter ultérieur de `SignalAndWait` ;
- les commandes socket Winsock sont traduites vers les ioctls POSIX et le
  socket VDP non implémenté est non bloquant ;
- l'audio hôte est arrêté avant la destruction des objets invités, ce qui
  supprime le timeout et l'arrêt forcé du worker audio.

## Preuve d'exécution

Le runner `tools/ac6_recomp_linux_oracle.py` vérifie avant lancement le XEX
complet (taille, SHA-256, XXH3), Title/Media ID et version via le manifest
qualifié, commit/arbre/rexglue, hashes de la pile et des sources, configuration
CMake, configuration runtime, arbre généré et binaire. Il verrouille le
checkout, refuse un output préexistant, inventorie le nombre de contrôleurs,
possède son Xvfb et son groupe de processus, et échantillonne
`CLOCK_MONOTONIC_RAW` sans dériver de cadence.

Run final sans contrôleur, contrôle négatif attendu :

- manifest local SHA-256 `fd41befe…13dec`, log `963711a1…2218e` ;
- binaire `f27b0f33…f590f`, XEX `6eefba42…cbbbc` ;
- 10 polls physiques observés avant la capture ;
- trois captures consécutives non noires, hashes
  `c5c61646…4c93`, `d3d79f6d…39f2`, `d169c278…fec6` ;
- `Execution complete`, arrêt audio naturel, `AC6_ORACLE_SHUTDOWN`, code 0.

Les images et le log restent locaux sous `/tmp/ac6-o1-qualified-clean-retry` ;
seuls leurs hashes et observations bornées sont durables.

## Validation

- build Clang 21 RelWithDebInfo `-j16` : pass ;
- CTest O1 : 3/3 (poll lifecycle/concurrence/retry, auto-reset, ioctl) ;
- tests runner lifecycle/concurrence/ownership : 5/5 ;
- `unittest` complet : 181/181 ; Pytest : 289/289 et 37 subtests ;
- Ruff et compilation Python ciblés : pass ;
- application indépendante des deux patches sur checkout exact : pass ;
- `git diff --check` : pass.

## Limites

O1 ne qualifie ni cadence, ni corpus mission, ni sémantique PAL. Aucun
contrôleur physique n'est présent sur l'hôte : le contrôle négatif est validé,
mais le profil à un contrôleur et le handoff interactif appartiennent à O2.
`JV`, `JP` et les six lanes du checkpoint 2 restent ouverts.

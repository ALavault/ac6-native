# AC6 retail NTSC-U/J — r420 — cause racine du blocage d'arrêt trouvée sans capture en direct : `native_guest_threads_stop_and_join()` fait un `thread.join()` inconditionnel et SANS DÉLAI sur chaque thread invité `ExCreateThread` ; `VulkanDevice` a un destructeur correct mais n'est jamais atteint

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r419 (nommé pour r420) : lire `native/src/
native_vulkan_backend.cpp` pour localiser la séquence de destruction
Vulkan et déterminer si `shutdown()` l'appelle réellement.

## Établi

### `VulkanDevice` a un destructeur RAII correct et complet

`native/src/native_vulkan_device.cpp:228-237` :

```c++
VulkanDevice::~VulkanDevice() noexcept {
  if (device_ != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(device_);
    vkDestroyDevice(device_, nullptr);
  }
  if (instance_ != VK_NULL_HANDLE) {
    vkDestroyInstance(instance_, nullptr);
  }
}
```

`NativeRuntime` possède ce périphérique via
`std::unique_ptr<VulkanDevice> offscreen_device_`
(`native_runtime.h:136`) — la destruction Vulkan devrait donc
s'exécuter automatiquement dès que l'objet `runtime` de
`ac6recomp_main.cpp` sort de portée, sans appel explicite requis.
**L'hypothèse de r419 (destruction Vulkan manquante) est donc
réfutée : le code de destruction existe et est correct.**

### La vraie cause : `shutdown()` reste bloqué AVANT d'atteindre ce point

`ac6recomp_main.cpp:202` appelle bien `runtime->shutdown()`.
`NativeRuntime::shutdown()` (`native_runtime.cpp:184-202`) appelle
**en premier**, avant toute autre étape :

```c++
native_guest_threads_stop_and_join();
```

Dont l'implémentation complète (`native_guest_threads.cpp`) est :

```c++
void native_guest_threads_stop_and_join() noexcept {
  g_stop.store(true, std::memory_order_release);
  std::vector<std::thread> threads;
  { std::lock_guard lock(g_mutex); threads.swap(g_threads); }
  for (auto& thread : threads) {
    if (thread.joinable()) thread.join();   // <-- SANS DÉLAI, inconditionnel
  }
}
```

**Chaque thread invité enregistré (créé par le stub `ExCreateThread`
généré, `tools/materialize_native_import_stubs.py:1056`, le seul
appelant de `native_guest_threads_register`) est rejoint via
`.join()` sans aucun délai d'attente.** Le mécanisme d'arrêt propre
(r277, confirmé r417) ne fonctionne QUE si le thread ciblé est
précisément arrêté À CE MOMENT dans l'un des stubs d'attente générés
qui vérifient `stop_requested()` et lancent `GuestThreadTerminated`
(établi r417/r419). **Si un seul thread invité enregistré est occupé
ailleurs au moment de l'arrêt — en train d'exécuter du PPC réel, ou
bloqué dans un appel hôte qui ne consulte pas ce drapeau — `.join()`
sur ce thread bloque INDÉFINIMENT, et `shutdown()` ne retourne
jamais.**

### Ceci explique tout ce qu'ont observé r418/r419 sans nécessiter de capture en direct

- `main()` reste bloqué à la ligne `runtime->shutdown()` (`:202`),
  **avant** que `runtime` (et donc `offscreen_device_`) ne sorte de
  portée — le destructeur `VulkanDevice` (correct) n'est simplement
  jamais atteint. Ceci explique pourquoi les threads pilote Vulkan
  (`vkcf`/`vkrt`/`vkps`, r419) restaient vivants indéfiniment, sans
  qu'aucun défaut n'existe dans le code de destruction lui-même.
- Le sondeur GPU (`NativeGuestVdService::poll_loop`, r292-r294, r418)
  est lancé via `std::thread([this]{poll_loop();}).detach()`
  (`native_guest_vd.cpp:181`) — **PAS** via
  `native_guest_threads_register` — il n'est donc PAS la cause directe
  de ce blocage précis (il n'est jamais rejoint), mais partage la même
  lacune structurelle : aucun mécanisme ne lui demande jamais de
  s'arrêter non plus.
- **Pourquoi ce blocage n'apparaissait jamais avant le correctif de
  r414** : avant r414, le boot n'atteignait jamais un état où
  suffisamment de threads invités s'exécutaient de façon soutenue et
  réaliste (r416/r417) pour qu'au moins un d'entre eux soit räisonnablement
  susceptible de se trouver hors d'un stub d'attente au moment précis
  où la fenêtre de sonde s'écoule — le mécanisme de jonction sans
  délai n'avait simplement jamais été mis en défaut de façon
  observable.
- **Pourquoi les captures gdb de r414-r417 semblaient "sortir
  normalement"** (déjà nommé comme lacune méthodologique par r419) :
  le `quit` de fin de script gdb termine l'inférieur de force,
  masquant ce blocage de `join()` sans jamais le révéler.

## Ce que ceci établit

**Blocage entièrement localisé, sans capture en direct nécessaire**
(la restriction `ptrace_scope` de ce bac à sable qui avait empêché
r419 d'inspecter le processus vivant n'était donc pas nécessaire —
la lecture du code source suffisait). Ce n'est ni un défaut Vulkan, ni
un défaut du chemin allocateur/boot (r399-r417, toujours résolu), ni
un défaut du mécanisme d'arrêt propre lui-même (r277/r417, dont la
logique reste correcte) — c'est une lacune de robustesse dans le
`.join()` sans délai de `native_guest_threads_stop_and_join()`, restée
invisible tant que le boot n'atteignait jamais un état où elle
pouvait se déclencher.

## Non établi

- **Lequel des threads invités enregistrés est effectivement hors
  d'un stub d'attente au moment du blocage**, et ce qu'il fait à ce
  moment précis — non identifié (nécessiterait soit une capture en
  direct malgré la restriction `ptrace_scope`, soit une trace de
  diagnostic supplémentaire côté C++ à ajouter).
- **Le correctif exact à appliquer** — un délai d'attente sur
  `thread.join()` (nécessite `std::thread` C++20+ ou une primitive
  d'attente bornée séparée, `std::thread::join()` n'a pas nativement
  de variante avec délai) est l'option la plus directe mais change le
  comportement observable en cas de blocage réel (fuite du thread
  plutôt que blocage) — pas conclu ni appliqué ce cycle.
- **Si le sondeur GPU détaché (`poll_loop`) devrait aussi être
  enregistré/joint** — lacune structurelle apparentée, non résolue.
- Aucun correctif proposé ni appliqué ce cycle.

## Décisions prises

- Ne pas relire l'hypothèse "destruction Vulkan manquante" de r419
  comme acquise — la vérifier d'abord par lecture directe avant de
  chercher plus loin, ce qui a réfuté cette hypothèse et mené
  directement à la vraie cause (précédent CLAUDE.md : corriger
  prédécesseurs, y compris ses propres hypothèses non vérifiées, par
  nom et numéro de cycle).
- Ne pas proposer de correctif au `.join()` sans délai sans d'abord
  identifier PRÉCISÉMENT quel thread bloque et pourquoi — un correctif
  à l'aveugle sur un mécanisme de synchronisation risquerait
  d'introduire une fuite de ressource ou une destruction prématurée
  d'un thread encore réellement actif (précédent r1111/r1113 : ne pas
  deviner sans preuve).

## Gate

Aucune source de production éditée ce cycle.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées dans le commit de ce cycle)
`ctest` relancé en entier.

## Named for r421

Identifier PRÉCISÉMENT quel thread invité enregistré est hors d'un
stub d'attente au moment du blocage — soit en ajoutant une trace de
diagnostic bornée (nom/adresse de routine `ExCreateThread`, horodatage
du dernier stub d'attente traversé) à `native_guest_threads_register`/
`stop_and_join`, soit en retentant l'attache `gdb -p` avec un
contournement légitime de la restriction `ptrace_scope` si
l'utilisateur l'autorise explicitement. Une fois identifié, concevoir
un correctif qui ne se contente pas d'un délai arbitraire sur
`.join()` mais traite la cause (le thread coincé lui-même, ou un stub
d'attente supplémentaire à instrumenter).

## Files

Aucun artefact gitignoré nouveau ce cycle (lecture de code source
uniquement).

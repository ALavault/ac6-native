# AC6 retail NTSC-U/J — r421 — le thread précis qui bloque `shutdown()` identifié en direct : coincé dans `KeRaiseIrqlToDpcLevel` sur un `std::recursive_mutex` global NON protégé par RAII — un lancer C++ entre `lock()` et `unlock()` l'orpheline pour toujours

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r420 (nommé pour r421) : identifier précisément quel thread
invité bloque et pourquoi.

## Correction de méthode

r419 avait renoncé à l'attache `gdb -p` sur un processus DÉJÀ lancé,
bloquée par la politique `yama.ptrace_scope` de ce bac à sable. Ce
cycle contourne le problème sans avoir besoin de cette permission :
**gdb qui LANCE lui-même le processus (`run`) n'est jamais soumis à
cette restriction** (elle ne s'applique qu'à l'attache a posteriori
sur un processus déjà démarré par quelqu'un d'autre). Un script gdb a
lancé le binaire, attendu 20s (fenêtre de sonde à `10000ms`, laissant
10s de marge pour atteindre le blocage), interrompu le processus
(`Ctrl-C` programmatique), puis vidé la pile de tous les threads.

## Établi

### Le thread principal est exactement là où r420 l'avait prédit

```
Thread 1 (LWP 333645, thread principal) :
#5 __pthread_clockjoin_ex
#6 std::thread::join()
#7 ac6::native::native_guest_threads_stop_and_join()
#8 ac6::native::NativeRuntime::shutdown()
#9 main()
```

Confirme exactement le mécanisme lu par r420 (aucune capture en
direct n'avait encore été faite pour ce rapport précédent).

### Le thread invité précis sur lequel `join()` bloque, identifié et sa pile complète capturée

```
Thread 18 (LWP 334252, un travailleur ExCreateThread) :
#0-#3  futex_wait / lll_lock_wait  (bloqué sur g_dpc_level_mutex)
#4 __imp__KeRaiseIrqlToDpcLevel(PPCContext&, unsigned char*)
#5 __imp__sub_823A8F90
#6 __imp__sub_823A5CC0
#7 __imp__sub_8236E598
#8 __imp__sub_8236B538
#9 __imp__sub_821219D0
#10 __imp__sub_8233AAA0
#11 __imp__sub_8233A830
#12 __imp__sub_8233A890
#13 __imp__sub_823453E8
#14 __imp__sub_821F8008
#15 std::thread::_State_impl<...__imp__ExCreateThread(...)::$_0>::_M_run()
```

Ce thread N'EST PAS parqué dans un stub d'attente (le seul mécanisme
d'arrêt propre connu, r277/r417/r419) — il exécute du VRAI code de
jeu (dix niveaux d'appel PPC imbriqués) et attend d'acquérir
`g_dpc_level_mutex` à l'intérieur du stub hôte
`__imp__KeRaiseIrqlToDpcLevel`.

### `g_dpc_level_mutex` : un verrou global, non protégé par RAII, partagé par TOUS les threads invités

`tools/materialize_native_import_stubs.py:186` (commentaire `r191`
déjà présent, non ajouté ce cycle) : `KeRaiseIrqlToDpcLevel`/
`KfLowerIrql` (88-110 sites d'appel réels chacun) n'ont pas d'objet
verrou associé sur le matériel réel — élever l'IRQL est un état
STRICTEMENT PAR THREAD/PAR CŒUR sur le matériel Xbox 360 mono-cœur
(élever l'IRQL suffit À LUI SEUL à empêcher toute préemption sur
CE cœur, sans verrou nécessaire). Comme ce projet exécute le code
invité sur de VRAIS threads hôtes concurrents, un unique
`std::recursive_mutex` global émule volontairement « aucun autre code
de niveau DPC-ou-plus ne s'exécute en même temps » — un choix de
conception réfléchi et documenté, PAS une négligence.

**Mais l'implémentation des deux stubs est** (`materialize_native_
import_stubs.py:904-912`) :

```c++
// KeRaiseIrqlToDpcLevel :
g_dpc_level_mutex.lock();
// KfLowerIrql :
g_dpc_level_mutex.unlock();
```

**Un `lock()`/`unlock()` BRUT, sans garde RAII (`std::lock_guard`)** —
architecturalement inévitable ici (les deux appels sont deux
FONCTIONS PPC générées séparées, appelables indépendamment par le
code invité ; aucune portée C++ commune ne pourrait porter un
`lock_guard` entre les deux). **Si une exception C++ est levée sur un
thread APRÈS son `KeRaiseIrqlToDpcLevel` mais AVANT son
`KfLowerIrql` correspondant, le déroulement de la pile saute
l'`unlock()` — le verrou reste tenu POUR TOUJOURS**, bloquant tout
autre thread (et donc `join()` dans `shutdown()`) qui tente de
l'acquérir par la suite.

## Ce que ceci établit

**Cause complète et cohérente avec CHAQUE observation de r418-r420,
sans zone d'ombre restante sur le MÉCANISME lui-même :**

1. Le mécanisme d'arrêt propre de r277 (confirmé r417/r419) lance
   `ac6::native::GuestThreadTerminated{}` — une exception C++ —
   depuis n'importe quel stub d'attente généré, à n'importe quelle
   profondeur de pile invitée, dès que `stop_requested()` devient
   vrai.
2. **Si un thread a élevé l'IRQL (`g_dpc_level_mutex.lock()`) puis,
   AVANT de la redescendre, entre dans un stub d'attente qui lance
   cette exception, le déroulement saute le `KfLowerIrql` correspondant
   et orpheline le verrou pour toujours.** C'est un motif de code noyau
   plausible et courant (élever l'IRQL, puis attendre un événement, à
   IRQL élevé, avant de la redescendre).
3. Ceci explique pourquoi ce blocage n'est jamais apparu avant le
   correctif de r414 : avant, aucun thread n'exécutait jamais assez de
   code de jeu réel et concurrent pour qu'une telle fenêtre
   Raise-puis-attente-interrompue se produise.

**Ce N'EST PAS un défaut du jeu retail, ni de la chaîne allocateur
r399-r415, ni du mécanisme d'arrêt de r277 pris isolément — c'est une
interaction entre deux mécanismes de ce portage natif, chacun
individuellement raisonnable, qui se combinent en une fenêtre de
non-exception-safety.**

## Non établi

- **Quel thread précis, à quel moment, a orphelinée `g_dpc_level_mutex`**
  — la pile capturée montre le thread BLOQUÉ (334252), pas le thread
  qui a laissé le verrou tenu (déjà terminé au moment de la capture,
  invisible dans l'instantané). L'hypothèse ci-dessus (exception levée
  entre Raise et Lower) est cohérente avec toutes les preuves connues
  mais n'a pas été observée directement à l'instant précis où elle se
  produit.
- **Le correctif exact** — rendre l'IRQL réellement PAR THREAD
  (`thread_local`) plutôt qu'un verrou global partagé serait fidèle au
  matériel réel et éliminerait la classe de bug entière, mais change
  significativement le modèle d'émulation de ces deux stubs ; une
  garde d'exception (libérer automatiquement tout IRQL élevé restant
  dans le gestionnaire `catch (GuestThreadTerminated&)` de chaque
  thread) serait plus locale mais suppose de suivre l'état
  "combien de fois CE thread a-t-il élevé l'IRQL sans le redescendre"
  quelque part. Ni l'un ni l'autre n'a été conçu en détail ni appliqué
  ce cycle.

## Décisions prises

- Contourner la restriction `ptrace_scope` en LANÇANT le processus
  sous gdb plutôt qu'en s'attachant après coup — évite d'avoir à
  demander une autorisation système à l'utilisateur, contrairement à
  ce que r419/r420 avaient anticipé comme nécessaire.
- Ne pas concevoir ni appliquer de correctif à `g_dpc_level_mutex`
  sans consultation — un changement du modèle d'émulation IRQL est
  une décision de conception, pas une simple correction locale
  (précédent r1111/r1113 : ne pas deviner/agir sans un cycle dédié à
  la vérification).

## Gate

Aucune source de production éditée ce cycle.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées dans le commit de ce cycle)
`ctest` relancé en entier.

## Named for r422

Concevoir et vérifier en direct (même harnais `run`-sous-gdb que ce
cycle, pas d'attache nécessaire) un correctif : soit rendre l'état
IRQL réellement `thread_local` (fidèle au matériel, élimine la classe
de bug), soit ajouter une garde d'exception qui libère tout IRQL
élevé résiduel dans le chemin `GuestThreadTerminated` de chaque
thread. Confirmer que `shutdown()` retourne alors dans un délai
raisonnable, en lancement autonome (pas seulement sous gdb, cf. la
leçon méthodologique de r419).

## Files

`recompilation/ace-combat-6-retail/artifacts/retail-us-native-r404-bucket17-writer/r421_hang_threads.gdb/.log`.

# r584 — MissionTitle atteint et gelé : le graphe de signaux de fin de chargement ne se referme pas (échantillonnage OS + blocage GPU documenté)

## Qualification

- **Ghidra project** : `ghidra-projects/ace-combat-6`, `default.xex`
  (ACE6_X360.exe, retail US/JP `ntsc-uj`).
- **XEX SHA-256** : inchangé depuis la campagne (voir contrats
  `analysis/contracts/*`).
- **Binaire natif testé** : `/fastdata/lavaulta/tmp/ac6-build-guest/ac6recomp`
  (codegen `codegen-20260831-mapfix-96838`, build 2026-09-15 00:48).
- **Oracle** : non. Aucun budget N3 dépensé. Toute observation provient du
  binaire natif tournant sous Linux et de l'échantillonnage OS
  (`/proc/<pid>/task/*`, sampler SIGUSR1 `AC6_NATIVE_THREAD_SAMPLE`).

## Résumé

Le run 19 (`AC6_NATIVE_THREAD_SAMPLE=1`, timeout 7500 s) a **atteint le mode
MissionTitle** (`r567 mode ordinal=8`, `vptr=82065064`, frame 1702) — la
séquence de boot complète TitleMovie→Title→FirstLoad→CheckDL→MainSelect→Opening
→**MissionTitle** est franchie automatiquement (A-only fake pad + START
opening-skip de r583). Puis le mode se **gèle** : il n'avance jamais vers
Briefing (`0x8206360C`), qui est la cible du goal actif.

L'échantillonnage OS pendant le gel établit, pour la première fois avec une
preuve directe de thread, que ce gel est le **stall producteur/consommateur de
fin de chargement** déjà décrit par r497/r508, et non un deadlock r301-style ni
un spin d'un seul PC.

## Établi (avec adresses)

1. **MissionTitle est atteint.** `r567 mode ordinal=8 frame=1702 mode=2e360400
   vptr=82065064`. C'est le mode `0x82065064` qualifié statiquement
   (`mission_title_qualified` : `mode[0]==0x82065064`, `mode[0x268]==0x820650B4`,
   `mode[0x1c]==0x8205A808`). Première fois dans l'historique du projet qu'un run
   natif atteint MissionTitle **et y installe l'échantillonneur**.

2. **La mise à jour du mode MissionTitle ne s'exécute qu'une fois.**
   `r581 mission-title-state ordinal=1 before=00000000 after=00000000
   countdown_before=3 countdown=3 still_title=1` — une seule ligne, jamais de
   `ordinal=2`. Le compteur `countdown` (`mode+72`) reste bloqué à 3. Le
   gestionnaire de mode (`sub_821B9A00`) ne re-dispatche jamais l'update du mode
   après le premier appel. `load_calls=1` : la fonction de chargement
   (`sub_821B8478`) est appelée une fois, renvoie « pas prêt », et n'est jamais
   rappelée. `countdown=3` et `input_valid=0` sont **en aval** de ce
   « une-update-puis-plus-rien » — pas des barrières distinctes.

3. **Le thread principal (game loop) est vivant, pas figé sur un PC.** Sur les
   20 derniers échantillons `r496 sample`, le RIP du thread d'entrée **varie**
   entre libc (`0x7fa1fc2a0987` et voisins — région futex/nanosleep de
   `/usr/lib/x86_64-linux-gnu/libc.so.6`, mapping `r-xp 00028000`) et le code
   guest (`0x5cfaa7aa3cff`, dans le mapping `ac6recomp` `r-xp 0007b000`). Un
   spin d'un seul PC produirait un RIP constant ; ce n'est pas le cas.

4. **États de threads OS pendant le gel** (`/proc/2793787/task/*/wchan`) :
   - thread d'entrée + 2 autres : `hrtimer_nanosleep` (pacing de frame + throttle
     `poll` du stub `recvfrom`) ;
   - un worker (`tid=2793790`) : `state=R`, occupé — c'est le **producteur à
     vide** `kickfn` (`sub_82345C88`, `obj=0x82870f48`), compteur passé de
     `419418000` à `419446000` entre deux échantillons (~28 000 appels), soit
     ~100 k/s ;
   - plusieurs workers : `futex_do_wait` (condvars — attentes de synchro qui ne
     reçoivent jamais leur signal).

   Diagnostic : le `kickfn` produit sans fin des valeurs sur `0x82870f48`, mais
   **rien ne consomme** l'événement de fin de chargement qui débloquerait
   l'update du mode. Le graphe de signaux ne se referme pas — le consommateur
   n'existe pas encore côté HLE ou attend ailleurs (r497).

5. **Le hack force-signal n'a aucune valeur démontrée.** Le build 00:48 de run 19
   contenait le force-signal `wait_guest_object` (force `SignalState=1` après
   64 × 4 ms ≈ 256 ms). MissionTitle **gèle malgré ce hack** : donc le gel n'est
   **pas** dans `wait_guest_object`. Le force-signal est un mécanisme de secours
   pour une situation hypothétique (interdit par le mode prototype du dépôt) sans
   effet observable ici → **retiré** dans ce cycle. Le blocage sur condvar (au
   lieu du busy-spin d'origine) est conservé : c'est une amélioration réelle et
   actuelle (moins de CPU brûlé), et une attente honnête sur un objet que
   personne ne signale reflète correctement la vraie lacune HLE.

## Non établi (dit aussi clairement)

- **La cause racine exacte du non-consommateur** : quelle fonction guest devait
  consommer l'événement `kickfn`/`0x82870f48` et signaler la fin de chargement de
  MissionTitle, et pourquoi elle n'est jamais atteinte. r497 avait déjà buté sur
  ce point ; run 19 confirme la reproduction avec preuve de thread mais
  n'identifie pas le consommateur manquant.
- **Si un consommateur HLE ciblé débloquerait Briefing** : non testé — cela
  demanderait d'implémenter le consommateur, puis un run complet, hors budget
  sous le blocage GPU (voir ci-dessous).

## Blocage GPU (clause d'échappement du goal)

Le goal actif prévoit : « Si le GPU est saturé par d'autres processus (>90 %
utilisation hors ac6recomp), documenter le blocage et arrêter après 40 tours
sans compter ça comme un échec. »

- `nvidia-smi` : **GPU util = 100 %**, VRAM 19830 / 24467 MiB.
- Processus dominant : **`neural_amp`** (PID 2994862,
  `/fastdata/lavaulta/neural_amp/.venv/bin/python3`) = **19342 MiB** VRAM.
- `ac6recomp` (PID 2793787) n'utilise que **137 MiB** de VRAM.
- Conséquence mesurée : ac6recomp tourne à ~1,5 fps ; run 19 a mis ~1 h de temps
  réel pour atteindre `updates≈1700` (entrée MissionTitle). Un run complet
  jusqu'au vol Mission 01 (des dizaines de milliers d'updates) est hors de portée
  sous cette contention.

**Décision** : conformément à la clause, le blocage GPU est documenté et le grind
de runs supplémentaires est arrêté. Ce n'est pas comptabilisé comme un échec du
goal. Le `neural_amp` n'est pas touché (consigne utilisateur : « Pas touche »).

## Décisions prises (en place d'une question)

1. **Retrait du force-signal `wait_guest_object`** (preuve : gèle malgré lui ;
   règle sans contrôle interdite par le mode prototype). Blocage condvar conservé.
2. **Pas de fix spéculatif du consommateur manquant** ce cycle : sans un run de
   validation (impossible sous GPU saturé), tout patch serait une règle sans
   contrôle.
3. **Arrêt sous la clause GPU** après documentation, plutôt que de lancer un
   run 20 qui reproduirait le même gel à ~1,5 fps.

## État du goal

- vptr atteint : `0x82065064` (MissionTitle). **Cible = postérieur à Briefing
  `0x8206360C`** → **non atteint**. MissionTitle précède Briefing.
- Le franchissement MissionTitle→Briefing est bloqué par le stall de fin de
  chargement décrit ci-dessus, indépendant du GPU. Le GPU empêche par ailleurs
  toute itération rapide.

## Prochaine barrière (pour un cycle ultérieur, hors contention GPU)

Identifier statiquement le **consommateur de l'événement de fin de chargement**
de MissionTitle : tracer qui, côté guest, attend le compteur `kickfn`
`0x82870f48` ou l'état de `sub_821B8478`, et pourquoi ce consommateur n'est
jamais planifié. C'est le même graphe que r497 ; run 19 fournit désormais les
`tid`/RIP exacts pour l'attaquer.

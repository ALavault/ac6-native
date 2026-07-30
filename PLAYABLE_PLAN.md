# PLAN — AC6 Native Playable

Écrit le 2026-07-30 après le cycle 324. Ce document est le **chemin critique**
vers un AC6 natif jouable. `DECOMPILATION_PLAN.md` reste la référence pour les
neuf phases de traduction et pour les inventaires ; ce plan-ci dit dans quel
ordre les franchir et à quelle preuve, parce que les neuf phases ne sont pas
indépendantes : une seule les bloque toutes aujourd'hui.

## 0. Résultat terminal

Livrer un exécutable Linux AMD64 natif qui, avec les seuls fichiers retail de
l'utilisateur (`default.xex`, `DATA.TBL`, `DATA00.PAC`, `DATA01.PAC`,
`bgmpack.bin`) :

- démarre sans émulateur, sans Xenia, sans firmware Xbox 360 ;
- atteint l'écran-titre, y répond à l'entrée, et traverse le sélecteur de
  campagne jusqu'à l'entrée 9 ;
- charge la première mission et présente un monde non vide ;
- vole : entrée clavier et manette, simulation à pas fixe, caméra, HUD ;
- produit du son ;
- s'arrête proprement, et échoue fermé sur données absentes ou corrompues.

Token terminal : `AC6_NATIVE_PLAYABLE_ACCEPTED`. Ne jamais l'écrire, ni employer
« jouable », « complet » ou « parité retail », avant que §9 passe en entier.

Vocabulaire : `recompiler-generated` n'est **jamais** `verified`. Un rapport
n'est pas une preuve. Un compteur à zéro n'est pas une absence tant que le canal
de mesure n'a pas été prouvé vivant — leçon du cycle 324.

## 1. État mesuré au cycle 324

Binaire `ad976caddc3b5600b566a02ef405f8c44f5e9c0e675da16d2a3fc07789edd055`,
sonde de 90 s.

| observable | valeur | lecture |
|---|---:|---|
| corpus généré | 23 321 implémentations, 50 unités | phase 1 largement acquise |
| `REX_FATAL` | **0** depuis le cycle 307 | plus aucun arrêt de traduction |
| threads | 33 | **corrigé au cycle 326** : les threads invités cumulent ~9 % du CPU, l'invité **attend** |
| lectures `DATA00.PAC` | 129, **toutes dans 1,1 s** | l'invité lit son index puis s'arrête |
| interruptions vblank (source 0) | 300 → **5 400**, ~60 Hz | l'hôte cadence correctement |
| interruptions EOP (source 1) | **12, gelé** | ← **le blocage** |
| `guest_swap_requests` | 4 | l'invité a demandé 4 images |
| `host_swap_presents` | 3 | l'hôte en a présenté 3 |
| présentation refusée, motif journalisé | 1 | à qualifier, pas à déduire |
| lignes de journal après t+1,5 s | **0** sur 88 s | l'invité n'appelle plus aucun service |

## 2. Le blocage, nommé mécaniquement

Tout passe par l'anneau de commandes PM4. Chaîne complète, lue dans le SDK :

```text
invité écrit des paquets dans l'anneau
invité écrit le pointeur d'écriture   -> CommandProcessor::UpdateWritePointer
                                         write_ptr_index_ = value ; event->Set()
worker CP  : tant que read_ptr == write_ptr, il tourne puis attend 5 ms
             sinon ExecutePrimaryBuffer(read, write)
  paquet PM4 INTERRUPT  -> DispatchInterruptCallback(1, cpu)  = EOP, source 1
  paquet PM4 XE_SWAP    -> IssueSwap(...)                     = présentation
```

Deux conséquences directes :

1. **`eop=12` signifie que l'invité a écrit 12 paquets `INTERRUPT` puis plus
   aucun.** Ce n'est pas un compteur hôte qui s'est arrêté : c'est une absence de
   paquets.
2. La branche **travail** du gestionnaire invité `sub_821E63B0` est réservée à la
   source 1 (cycle 317 §1). C'est donc le seul chemin qui peut dessiner, et il est
   alimenté par ce que l'invité soumet. L'état observé est une boucle refermée
   sur elle-même : pas de soumission → pas d'EOP → la branche travail ne tourne
   pas → pas de soumission.

Il faut donc trancher, par mesure, entre trois causes exclusives :

- **(a)** l'invité cesse d'appeler `UpdateWritePointer` — le défaut est en amont,
  dans la logique invitée ;
- **(b)** l'invité continue d'appeler `UpdateWritePointer` mais l'hôte ne
  consomme pas — le défaut est dans le worker CP ou l'anneau ;
- **(c)** l'hôte consomme mais n'exécute plus de paquets `INTERRUPT`/`XE_SWAP` —
  le défaut est dans le décodage des paquets.

Aucune de ces trois hypothèses n'a jamais été mesurée. C'est P0.

## 3. Contraintes de la plateforme de mesure

Établies au cycle 324, elles conditionnent la méthode de tout le plan.

- **Levé au cycle 326 par l'opérateur** : `kernel.perf_event_paranoid=1` et
  `kernel.yama.ptrace_scope=0`. `perf record --pid` et l'unwinding DWARF
  fonctionnent, et sont désormais la première mesure à tenter sur un blocage.
  Attention : un profil par symbole sans **répartition par thread** est
  trompeur — au cycle 326 les 51 % de `WaitMultiple` se sont avérés être un
  thread hôte, pas l'invité.
- `eu-stack -p` échoue par timeout sur ce binaire LTO de 165 Mo. Ne pas y revenir.
- `gdb` sur ce binaire LTO de 165 Mo passe des minutes à charger ses symboles.
- Conséquence : **la mesure vit dans le runtime**, dans des compteurs en arbre
  gardés par cvar. `gdb --args` reste la solution de dernier recours pour une
  pile invitée, à budgéter explicitement.
- Toute sonde passe par `tools/ac6-frame-loop-probe.sh`, qui impose
  `--ac6_performance_mode=false` (sans quoi le runtime coupe son propre journal),
  borne réellement le processus invité, et nettoie les fuites `/dev/shm`.

## 4. Phase P0 — la boucle de trame vit

Seule phase active. Rien d'autre ne peut avancer avant elle.

- **P0.1 — instrumenter la frontière de soumission. FERMÉE au cycle 325 :
  cause (a).** Six compteurs d'anneau posés dans l'arbre. Mesuré, gelé dès t+5 s
  et inchangé 70 s plus tard : `wptr_updates=20`, `wptr == rptr == 0x43`,
  `primary_executions=11`, `pm4_interrupt=12`, `pm4_swap=4`.
  L'anneau est **entièrement drainé** — l'hôte a consommé tout ce qui lui a été
  soumis — et l'invité **ne réécrit plus** le pointeur d'écriture. `pm4_interrupt`
  concorde avec `eop` et `pm4_swap` avec `guest_swap_requests`, donc le décodage
  n'a rien perdu. **(b) et (c) sont réfutées par mesure.** Toute la famille des
  hypothèses côté hôte est close : ni interruptions, ni worker CP, ni décodage
  PM4, ni présentation ne retiennent quoi que ce soit.
  Voir `reports/cycle-325-p0-1-guest-stopped-submitting.md`.
- **P0.2 — sur quoi les threads invités attendent-ils ? FERMÉE au cycle 327.**
  Recensement des attentes invitées dans l'arbre. Sur 70 s : **17 threads garés,
  2 vivants**. Sept garés ont `waits=1` — entrés une fois au démarrage, jamais
  revenus — dont six sur des handles distincts régulièrement espacés
  (`F8000030/3C/48/54/5C/74`), signature d'un pool de travail jamais signalé.
  `tid=0010` est le ping-pong des cycles 320-323 : `waits=26`, il a cyclé
  26 fois puis s'est garé, et **un seul** thread y est garé là où le cycle 320 en
  voyait deux — avec la valeur à `0` et le worker attendant `1`, c'est le
  protocole **correct**. Résultat central : **le thread principal est absent du
  recensement.** Voir `reports/cycle-327-guest-wait-census.md`.
- **P0.2 bis — où est le thread principal ? FERMÉE au cycle 328.** Il n'était
  dans aucun des trois candidats proposés : il était bloqué dans un **appel
  système hôte**, `recvfrom(fd, buf, 1281, 0, ...)`, à **0,0 % de CPU**,
  invisible à tout recensement interne au noyau invité. Mesuré depuis
  `/proc/<pid>/task` — sans reconstruction — puis attribué à une **cascade de
  trois divergences hôte** : le port privilégié 999 refusé par Linux (`EACCES`,
  la 360 n'a pas cette règle), puis, une fois le `bind` réussi, le `FIONBIO`
  Winsock transmis tel quel à `ioctl()`, puis l'argument big-endian de l'invité.
  Corrigé dans `XSocket` sans aucun privilège requis. Effet mesuré sur deux
  exécutions : thread principal **0,0 % bloqué -> 121 % actif**,
  `host_swap_presents` **3 -> 12**, `eop` **12 -> 34**, `wptr`
  **`0x43` -> `0x9D`**. Voir `reports/cycle-328-main-thread-blocked-in-a-socket-receive.md`
  et `patches/rexglue-guest-socket-privileged-port-and-fionbio-20260730.patch`.
- **P0.2 ter — quelle boucle le thread principal exécute-t-il ? FERMÉE au
  cycle 329. Ce n'est pas une boucle : c'est une faute répétée.** `perf annotate`
  sur la fonction chaude donne **100,00 % des échantillons sur une seule `ud2`**.
  Le générateur a traduit un `bctr` de table de saut en `switch` sur l'**adresse**
  chargée avec des cas ordinaux (`cmpl $0x2`) : la comparaison ne peut jamais
  être vraie pour une adresse `>= 0x82000000`, donc seul le `default:
  __builtin_trap()` est atteignable. Étendue : **9 aiguillages dégénérés sur 751**,
  tous énumérés. Défaut aggravant, corrigé : le repli du gestionnaire de signaux
  **retournait sans avancer le PC ni réémettre**, ce qui réexécute l'instruction
  fautive indéfiniment — 121 % de CPU, aucun plantage, aucun message.
  Voir `reports/cycle-329-guest-spins-on-a-ud2.md`.
- **P0.2 quater — faire sauter ce `bctr`. OUVERTE, et c'est le blocage courant.**
  Lire la table de saut à `0x8267A1D0`, énumérer ses cibles réelles, comprendre ce
  qui distingue ces 9 aiguillages des 742 corrects, et traiter les 8 autres au
  même passage. Coût : une régénération du corpus, pas un correctif d'exécution.
- **P0.3 — attribuer et refermer.** Une fois la boucle nommée, remonter
  statiquement depuis le corpus généré à ce dont elle dépend. Régression isolée
  avant toute correction — règle 22 du plan racine. Un seul lot causal, effet
  mesuré déclaré, y compris nul.

**Porte P0** : sur une exécution de 60 s, sans `REX_FATAL` :
`write_ptr` avance continûment, `eop` croît de façon monotone au-delà de 100, et
`host_swap_presents` dépasse **600** (soit ≥ 10 images/s soutenues). Une porte
franchie une fois n'est pas franchie : elle doit être reproductible par
`ac6-frame-loop-probe.sh`.

## 5. Phase P1 — écran-titre et menu

Prérequis : P0.

- P1.1 — le contenu présenté change au fil du temps : capturer des images à
  intervalles et prouver qu'elles diffèrent. Une image fixe non vide n'est pas un
  écran-titre.
- P1.2 — l'entrée modifie l'état présenté. Le cycle 314 avait envoyé `Return`,
  `space`, `KP_Enter` par `xdotool` sans changer un seul pixel ; ce test doit
  passer, pas seulement être rejoué.
- P1.3 — traverser jusqu'au sélecteur de campagne. Les rapports existants
  (`CAMPAIGN_SELECTOR_ONE_ORIGIN_BOUNDARY.md`,
  `CAMPAIGN_MISSION_LOAD_PATH_REDUCTION.md`) sont des acquis statiques à
  consommer, pas à refaire.

**Porte P1** : trois images distinctes horodatées, un changement d'état
attribuable à une entrée nommée, et le sélecteur atteint.

## 6. Phase P2 — entrée 9, chargement de mission, monde non vide

Prérequis : P1.

Acquis statiques à consommer : `ENTRY9_SCENE_PATH_TABLE_REPORT.md`,
`ENTRY9_SCENE_RESOURCE_RESOLUTION_REPORT.md`,
`ENTRY9_X360_UNIT_MANAGER_REPORT.md`, `MISSION_VISUAL_BOOTSTRAP_REPORT.md`,
`DPL_ARCHIVE_HANDLE_CHAIN.md`, `FHM_ASSET_MANIFEST_REPORT.md`.

- P2.1 — l'entrée 9 est sélectionnée par le **chemin retail**, pas forcée.
- P2.2 — les ressources de la mission sont résolues et chargées depuis les
  conteneurs retail, champs inconnus préservés.
- P2.3 — le monde est peuplé : compter les entités créées, pas les octets lus.

**Porte P2** : le **runtime natif** — et non `scene_shell` — charge la première
mission avec un état non vide prouvé, énuméré et haché. Le shell de diagnostic ne
ferme aucune porte de ce plan.

## 7. Phase P3 — renderer complet pour la première mission

Prérequis : P2.

- P3.1 — inventorier les shaders Xenos atteignables dans la mission 1.
- P3.2 — les traduire par XenosRecomp, préserver la sémantique fetch/constantes
  et l'endianness.
- P3.3 — cibles de rendu, profondeur, états vertex/index/stream, matériaux,
  textures, soumission. Les acquis `MATE_*`, `NDXR_*`, `NSXR_*`, `NTXR_*`,
  `AC6_MATERIAL_TEXTURE_LINK_REPORT.md` sont des entrées.

**Porte P3** : zéro gestionnaire de shader ou d'état atteignable manquant, et un
manifeste de trame déterministe reproductible entre deux exécutions.

## 8. Phases P4 à P7

- **P4 — vol.** Entrée clavier et manette reliées au propriétaire joueur prouvé
  (`FUNCTION_821CE088_PLAYER_INPUT_REPORT.md`,
  `FUNCTION_821BE268_DEFAULT_BINDINGS_REPORT.md`), simulation à pas fixe, caméra,
  HUD, pause. **Porte** : un segment de la première mission avance sous entrée
  native avec un état stable.
- **P5 — audio.** XMA, ordonnancement de décodage, `bgmpack.bin`.
  **Porte** : sortie audible en mission, timing de paquets stable, aucune
  dépendance émulateur.
- **P6 — parité différentielle bornée.** Points de contrôle : démarrage, titre,
  chargement de mission, jeu. Xenia est un **oracle**, jamais une dépendance.
  **Porte** : chaque point de contrôle vert, ou déviation bornée explicitement
  approuvée comme changement de port.
- **P7 — durcissement produit.** Build normal et ASan/UBSan, rejets fail-closed
  sur données absentes/tronquées/non supportées, installation racine, paquet sans
  asset retail. **Porte** : les cinq résultats produit de
  `docs/native-port-acceptance.md` exécutés et enregistrés.

## 9. Porte finale

Toutes obligatoires avant le token :

- P0 à P7 franchies, chacune reproductible par une commande nommée ;
- build Linux propre, plus build et tests ASan/UBSan ;
- aucun service noyau, XAM ou XMA atteint qui soit un stub permissif ;
- données retail absentes, tronquées ou non supportées rejetées fermées ;
- paquet sans asset retail, sans XEX, sans firmware, sans dépendance émulateur ;
- une mission réelle jouée sous entrée native, sur une session bornée, avec
  verdict humain binaire ;
- `DECOMPILATION_PLAN.md`, `CURRENT.json` et ce plan cohérents.

Toute case ouverte interdit le token.

## 10. Discipline propre à ce plan

Les règles 22 à 27 du `PLAN.md` racine s'appliquent. S'y ajoutent :

1. **Prouver le canal avant de conclure d'une absence.** Un `0` ne vaut que si le
   compteur a déjà été vu non nul dans la même configuration.
2. **Un compteur nomme son étage.** `guest_swap_requests`, `host_swap_presents` et
   `pacing_notifications` sont trois étages distincts ; les confondre a produit
   une fausse régression au cycle 324 et une fausse absence de défaut au
   cycle 316.
3. **La mesure vit dans l'arbre**, jamais dans un correctif local : le cycle 323 a
   trouvé trois correctifs d'instrumentation archivés et non appliqués dont
   dépendaient les conclusions de deux cycles.
4. **Un cycle nomme une entrée, une frontière, une sortie mesurable.** Pas de
   cycle dont l'unique artefact enveloppe le précédent.
5. **Ne pas franchir une phase par le shell de diagnostic.** `scene_shell` est un
   instrument, pas le produit.
6. Après deux cycles sans progrès mesurable sur deux hypothèses distinctes,
   classifier et changer de frontière. Ne pas répéter.

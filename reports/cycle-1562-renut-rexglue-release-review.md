# Cycle 1562 — reNut : audit ReXGlue, Linux et releases

Audit public réalisé le 12 août 2026, sans ISO, XEX, save, profil ou actif
retail. Aucun binaire reNut n'a été exécuté. Les deux paquets officiels
Linux ont été extraits en lecture seule ; le ZIP Windows a seulement été
inventorié et ses binaires inspectés statiquement.

## Décision pour AC6 Mission 01

reNut ne fournit aucune sémantique `retail-qualified`. Il apporte toutefois
quatre règles directement utiles au bring-up `provisional-rexglue` d'AC6 :

1. **Un `PRESENT` ReXGlue n'est pas une horloge invitée.** Au pin SDK visé par
   la build Linux, une pipeline asynchrone non prête supprime le draw ; le
   backend Vulkan peut ensuite supprimer la présentation de toute la frame.
   Le replay AC6 doit donc rester indexé par poll/marqueur invité, avec
   `present_index` comme télémétrie seulement.
2. **Une capture visuelle oracle exige l'état effectif du compilateur de
   shaders.** Imposer `async_shader_compilation=false`, ou prouver un warm-up
   sans placeholder/draw/frame supprimé ; sceller le cvar, le cache pipeline,
   le backend et le binaire dans le reçu. Une capture à froid asynchrone ne
   peut pas entrer dans M01-F.
3. **Un hang Linux ReXGlue peut être une faute hôte répétée.** Le pin public
   laisse un `SIGSEGV` non pris en charge retourner dans l'instruction fautive.
   Les runs oracle doivent conserver watchdog, backtrace et consommation CPU ;
   une fenêtre vivante ne prouve pas que le guest progresse.
4. **Le codegen de secours n'est jamais du C++ produit.** reNut suit 3 Mo de
   fonctions PPC recopiées pour contourner des opcodes manquants. L'audit de
   frontière AC6 doit continuer à refuser `ppc_recomp`, mappings et fonctions
   générées, même cachés dans un fichier au nom manuel.

Les correctifs POSIX/audio ci-dessous sont de bons tests de non-régression
pour un oracle Linux ReXGlue. Ils ne doivent pas être copiés dans le jeu C++
natif et ne ferment aucune lane du checkpoint 2.

## Taxonomie

| Classe | Usage ici |
|---|---|
| `provisional-rexglue` | comportement hôte vérifié dans un SDK public épinglé, utilisable pour configurer et observer l'oracle |
| `retail-qualified` | preuve rattachée à un XEX exact et à l'exécution Xbox 360 ; ensemble vide |
| `divergent` | cheat, clock hôte, bypass input, no-op, erreur runtime ou comportement différent du chemin invité |
| `documented-unmatched` | source, release, option ou statut sans chaîne publique permettant de les relier exactement |

## Provenance source et release

### Branche principale

| Élément | Valeur |
|---|---|
| dépôt | [`masterspike52/reNut`](https://github.com/masterspike52/reNut) |
| HEAD `main` | [`ef74a036676db6a71c3e2e93bd770e64cf539e5a`](https://github.com/masterspike52/reNut/commit/ef74a036676db6a71c3e2e93bd770e64cf539e5a), arbre `057f651215c3503dfc6799380741e351f0927c55` |
| historique | 199 commits, 17 tags visibles, aucun sous-module |
| taille suivie | 36 fichiers, 3 492 647 octets |
| licence racine | absente ; GitHub renvoie `license=null` |

Le README exige la version US sans update
([lignes 49–60](https://github.com/masterspike52/reNut/blob/ef74a036676db6a71c3e2e93bd770e64cf539e5a/README.md#L49-L60)),
mais ne donne ni SHA-256, ni Media ID, ni version/base version. Le manifeste ne
nomme que `assets/default.xex`
([lignes 1–10](https://github.com/masterspike52/reNut/blob/ef74a036676db6a71c3e2e93bd770e64cf539e5a/renut_manifest.toml#L1-L10)).
L'identité retail n'est donc pas reproductible.

La pile SDK de `main` est incohérente : le manifeste annonce `0.8.0`, tandis
que le CMake généré recherche `0.7.4` en l'absence d'override
([lignes 5–25](https://github.com/masterspike52/reNut/blob/ef74a036676db6a71c3e2e93bd770e64cf539e5a/generated/rexglue.cmake#L5-L25)).
Il n'existe ni lockfile, ni gitlink, ni SHA de SDK.

### Source Linux publique, mais non rattachée à la release

La branche distante `renut_linux` est un commit racine sans parent :
[`e73ca783f2ea269e65c6b30dba52634cb0c86233`](https://github.com/masterspike52/reNut/commit/e73ca783f2ea269e65c6b30dba52634cb0c86233),
arbre `7d335a4041b2be496a792b3ed1d22184921b4db1`. Elle n'est ni ancêtre ni
descendante de `main`. Elle contient le CMake portable, le packaging AppImage,
les correctifs Linux et les hooks clavier/souris absents du HEAD principal.

Le Dockerfile vise le tag SDK
`nightly-20260809-f5c85215`
([lignes 90–103](https://github.com/masterspike52/reNut/blob/e73ca783f2ea269e65c6b30dba52634cb0c86233/packaging/container/Dockerfile#L90-L103)),
qui résout aujourd'hui au commit ReXGlue
[`f5c85215174c9dcd67b4c77227a979c4fc33197a`](https://github.com/rexglue/rexglue-sdk/commit/f5c85215174c9dcd67b4c77227a979c4fc33197a),
arbre `12076e884ca6b7974d5f4ddc4aa7b7876ce5ec1e`. Ce pin est utile, mais le
binaire livré s'identifie seulement comme
`rexglue-v0.9.0.0-dev.unknown-RelWithDebInfo` : aucun commit ne lui est lié.

Surtout, le commit Linux ignore tout `generated/` mais son `CMakeLists.txt`
inclut `generated/rexglue.cmake`
([lignes 7–13](https://github.com/masterspike52/reNut/blob/e73ca783f2ea269e65c6b30dba52634cb0c86233/CMakeLists.txt#L7-L13)).
Le script de build lance directement CMake, sans `rexglue migrate` ni codegen
([lignes 112–122](https://github.com/masterspike52/reNut/blob/e73ca783f2ea269e65c6b30dba52634cb0c86233/packaging/container/build.sh#L112-L122)).
Un checkout propre échoue donc à la configuration : fichier inclus absent,
puis commande `rexglue_setup_target` inconnue. La release a nécessairement
été construite depuis un worktree contenant des fichiers ignorés.

### Paquets officiels

Deux releases pointent toutes deux sur le tag/commit
`d33da27cc0207fca01a4924d95083e692bebf7bb`, qui ne contient pas la branche
Linux :

| Release / asset | Taille | SHA-256 |
|---|---:|---|
| [`finally`](https://github.com/masterspike52/reNut/releases/tag/finally) / `reNut-x86_64.AppImage` | 25 627 128 | `810d22304fa275d5d4a30578799070a2dfab9061f4c65c74634951cf274a3407` |
| [`mnk`](https://github.com/masterspike52/reNut/releases/tag/mnk) / `reNut-x86_64.AppImage` | 25 627 128 | `358babce5c306cabb3048e1db418808a4cf8e122ca1a30a43354d9ceae73f4b6` |
| `renut-win-x64.zip`, identique dans les deux releases | 30 090 352 | `b9fe172af5bc7d609e0bf4bb7be8ed12c9cf9f7edef623316444d7a087a2f6f0` |

Les deux AppImages ont des enveloppes différentes. Neuf des onze fichiers du
SquashFS sont byte-identiques, notamment l'exécutable, les cinq bibliothèques,
`extract-xiso`, `AppRun` et le descripteur desktop ; seuls l'icône et son alias
`.DirIcon` diffèrent. Le `AppRun` extrait vaut
`7391c2c987ebfafd1bf693eb3b25b425a105e360c2e50a69e48cb838a06e17a6`
et correspond byte à byte à celui du commit Linux. Cela soutient l'inférence
que cette branche a servi au packaging ; ce n'est pas un reçu de build.

Le ZIP Windows utilise un autre runtime : `v0.8.0.0-dev.unknown`, et ses DLL
contiennent des chemins absolus vers un arbre `rexglue-ostentation-solar` non
épinglé. Windows et Linux ne sont donc pas deux builds interchangeables du
même oracle. Une qualification doit sceller plateforme, exécutable, DLL/SO,
backend, cvars et configuration.

## Renderer : pourquoi une capture à froid est invalide

Le projet définit `sync_shader_compile=true` et l'inverse dans le cvar SDK
([lignes 209–221](https://github.com/masterspike52/reNut/blob/e73ca783f2ea269e65c6b30dba52634cb0c86233/src/renut_engine/frameHooks.cpp#L209-L221)).
Son commentaire affirme qu'une pipeline asynchrone non prête fait disparaître
le draw. Le SDK public visé confirme exactement le comportement :

- le cvar est asynchrone par défaut et avertit d'artefacts brefs
  ([`command_processor.cpp`, lignes 72–81](https://github.com/rexglue/rexglue-sdk/blob/f5c85215174c9dcd67b4c77227a979c4fc33197a/src/graphics/command_processor.cpp#L72-L81)) ;
- D3D12 retourne avec succès avant le draw lorsque la pipeline n'est pas prête
  ([lignes 2393–2404](https://github.com/rexglue/rexglue-sdk/blob/f5c85215174c9dcd67b4c77227a979c4fc33197a/src/graphics/d3d12/command_processor.cpp#L2393-L2404)) ;
- Vulkan marque la frame et retourne avant le draw si la pipeline est un
  placeholder
  ([lignes 3850–3869](https://github.com/rexglue/rexglue-sdk/blob/f5c85215174c9dcd67b4c77227a979c4fc33197a/src/graphics/vulkan/command_processor.cpp#L3850-L3869)) ;
- avec le réglage Vulkan par défaut, la présentation entière d'une frame qui a
  utilisé un placeholder est supprimée
  ([définition](https://github.com/rexglue/rexglue-sdk/blob/f5c85215174c9dcd67b4c77227a979c4fc33197a/src/graphics/vulkan/command_processor.cpp#L58-L61),
  [usage](https://github.com/rexglue/rexglue-sdk/blob/f5c85215174c9dcd67b4c77227a979c4fc33197a/src/graphics/vulkan/command_processor.cpp#L2292-L2305)).

Le README recommande pourtant de décocher `sync_shader_compile`
([lignes 84–99](https://github.com/masterspike52/reNut/blob/ef74a036676db6a71c3e2e93bd770e64cf539e5a/README.md#L84-L99)),
ce qui réactive l'asynchrone. Le ZIP Windows livre au contraire
`async_shader_compilation=false`. Cette contradiction est
`documented-unmatched` pour le binaire ; elle justifie une lecture de l'état
effectif, jamais une hypothèse tirée du README.

Conséquences AC6 :

- `present_index` reste diagnostique et ne peut ni découper ni cadencer une
  trace d'inputs ;
- le reçu visuel doit inclure compteurs de pipelines pending/placeholders,
  draws supprimés et presents supprimés ;
- M01-F exige deux exécutions process-neuf identiques avec compilation
  synchrone, ou un warm-up scellé et rejoué avant la fenêtre comparée ;
- ces règles qualifient l'observation ReXGlue, pas le rendu retail Xbox 360.

## Cadence et input

Le limiteur reNut utilise `std::chrono::steady_clock`, la fréquence de l'écran,
un waitable timer Windows et une boucle d'attente
([lignes 63–164](https://github.com/masterspike52/reNut/blob/ef74a036676db6a71c3e2e93bd770e64cf539e5a/src/renut_engine/frameHooks.cpp#L63-L164)).
C'est un mécanisme de présentation hôte `divergent`, pas une preuve de tick
guest 30/60 Hz. Il ne doit alimenter ni le census de cadence v3 ni le replay
natif.

Le support clavier/souris Linux est un autre contre-exemple utile. Il :

- fabrique un pad connecté et transforme un échec XInput en succès ;
- écrit ensuite directement dans le bloc float post-traité du jeu, sans
  quantification, deadzone ou courbe XInput
  ([lignes 783–826](https://github.com/masterspike52/reNut/blob/e73ca783f2ea269e65c6b30dba52634cb0c86233/src/renut_engine/mnk_controls.cpp#L783-L826)) ;
- produit les répétitions de touches avec `steady_clock`
  ([lignes 632–658](https://github.com/masterspike52/reNut/blob/e73ca783f2ea269e65c6b30dba52634cb0c86233/src/renut_engine/mnk_controls.cpp#L632-L658)).

Cette voie est `divergent` pour un replay manette. Elle confirme le seam AC6 :
enregistrer/rejouer chaque entrée `XamInputGetState`, avant les adaptations
spécifiques au jeu, et désactiver les injections clavier/souris pendant la
qualification.

## Trois défauts Linux ReXGlue recoupés

### Sauvegardes POSIX

Le pin SDK combine `O_RDONLY`, `O_WRONLY` et `O_RDWR` au moyen d'un OR
([lignes 168–200](https://github.com/rexglue/rexglue-sdk/blob/f5c85215174c9dcd67b4c77227a979c4fc33197a/src/core/filesystem_posix.cpp#L168-L200)).
Ces valeurs forment un champ mutuellement exclusif, pas des bits indépendants ;
read+write devient `O_WRONLY`. reNut interpose une sélection exacte de mode et
ramène les nombres d'octets à zéro sur erreur
([lignes 115–149](https://github.com/masterspike52/reNut/blob/e73ca783f2ea269e65c6b30dba52634cb0c86233/src/renut_engine/linuxfixes/posix_file_access.cpp#L115-L149)).

Pour AC6, toute version ReXGlue Linux candidate doit tester ouvrir-écrire-relire,
flush, reprise, corruption et save atomique. Ce correctif hôte est
`provisional-rexglue`, pas une sémantique retail.

### Course XAudio unregister/submit

Le worker SDK copie callback/argument sous verrou, libère le verrou puis exécute
le callback invité
([lignes 128–150](https://github.com/rexglue/rexglue-sdk/blob/f5c85215174c9dcd67b4c77227a979c4fc33197a/src/audio/audio_system.cpp#L128-L150)).
`UnregisterClient` peut entre-temps détruire le driver
([lignes 267–283](https://github.com/rexglue/rexglue-sdk/blob/f5c85215174c9dcd67b4c77227a979c4fc33197a/src/audio/audio_system.cpp#L267-L283)),
alors que `SubmitFrame` ne dispose que d'assertions avant le déréférencement
([lignes 251–265](https://github.com/rexglue/rexglue-sdk/blob/f5c85215174c9dcd67b4c77227a979c4fc33197a/src/audio/audio_system.cpp#L251-L265)).

reNut interpose les trois exports et attend une accalmie bornée avant
unregister
([lignes 245–249](https://github.com/masterspike52/reNut/blob/e73ca783f2ea269e65c6b30dba52634cb0c86233/src/renut_engine/linuxfixes/audio_client_guard.cpp#L245-L249)).
Le commentaire de projet rapporte 37 ms observés, sans fixture ni trace
publique. La course source est confirmée ; la durée et l'efficacité du patch
restent `documented-unmatched`.

AC6 doit sceller compteurs register/unregister/submit/drop et refuser toute
capture audio contenant un drop. Le fallback `SDL_AUDIODRIVER=dummy` reste la
configuration headless qualifiée ; le choix PulseAudio reNut est propre à son
desktop.

### Gestion des fautes hôte

Le handler POSIX SDK appelle ses handlers puis retourne silencieusement si
aucun ne revendique la faute
([lignes 159–208](https://github.com/rexglue/rexglue-sdk/blob/f5c85215174c9dcd67b4c77227a979c4fc33197a/src/core/exception_handler_posix.cpp#L159-L208)).
Pour un `SIGSEGV`, le noyau reprend alors l'instruction fautive. Le commentaire
reNut décrit un thread audio à 100 % et une fenêtre encore vivante
([lignes 31–44](https://github.com/masterspike52/reNut/blob/e73ca783f2ea269e65c6b30dba52634cb0c86233/src/renut_engine/linuxfixes/audio_client_guard.cpp#L31-L44)).

C'est un défaut hôte `divergent`. Avant de promouvoir un oracle natif Linux,
un test synthétique doit prouver qu'une faute non traitée termine le processus
avec diagnostic au lieu de devenir un hang.

## Codegen recopié, cheats et sémantique

[`mullhwucrash.cpp`](https://github.com/masterspike52/reNut/blob/ef74a036676db6a71c3e2e93bd770e64cf539e5a/src/renut_engine/fixes/mullhwucrash.cpp)
fait 47 273 lignes et 3 010 314 octets, SHA-256
`0067c041e6103250c2f9e6a8ff1ead125ef026f801429ef46baed71796e32539`.
Il contient dix-huit fonctions `REX_HOOK_RAW` dont le corps est une sortie
PPC recompilée littérale. L'historique l'associe notamment à des contournements
`vsldoi128`, sans test d'instruction ni preuve de frontière. C'est du code
généré dérivé, quelle que soit l'extension ou le chemin du fichier.

Le projet expose en outre des hooks `Infinite_health`, `Infinite_fuel_and_ammo`,
`No_Timer` dans sa configuration et `StopNSwap` modifie directement les flags
guest
([lignes 19–56](https://github.com/masterspike52/reNut/blob/ef74a036676db6a71c3e2e93bd770e64cf539e5a/src/renut_engine/StopNSwap.cpp#L19-L56)).
Ces voies sont `divergent` et doivent être absentes d'un profil oracle AC6.

## Import, paquet et licences

`AppRun` trouve le premier `default.xex` jusqu'à trois niveaux, sans identité,
région ou hash, puis remplace le répertoire de jeu et écrit directement la
configuration
([lignes 92–152](https://github.com/masterspike52/reNut/blob/e73ca783f2ea269e65c6b30dba52634cb0c86233/packaging/AppRun#L92-L152)).
Il s'agit d'un flux convivial, pas d'un import fail-closed. AC6 conserve le
cache v2 atomique, l'identité PAL et l'interdiction de relire les PAC après
import.

Le paquet Linux contient l'exécutable, trois bibliothèques, `libstdc++`,
`libgcc`, `extract-xiso`, l'icône et les launchers ; aucun LICENSE/NOTICE. Le
ZIP Windows ajoute deux PNG de remplacement de texture sans manifeste de
provenance. Aucun XEX/ISO/STFS ou conteneur retail autonome n'a été trouvé par
inventaire et recherche de signatures. Toutefois les exécutables contiennent
le code recompilé et des constantes d'un XEX US non qualifié : ils ne sont pas
des composants admissibles du paquet natif AC6.

L'absence de licence racine, de notices dans les releases et de provenance des
PNG interdit toute copie de code ou d'actifs. Seules les formes architecturales
et les tests négatifs sont retenus.

## Matrice d'adoption AC6

| Observation reNut | Classe | Action AC6 |
|---|---|---|
| draw/present supprimé pendant compilation async | `provisional-rexglue` | sceller async=false ou warm-up + compteurs zéro ; present seulement télémétrie |
| limiteur `steady_clock`/fréquence moniteur | `divergent` | ne jamais en déduire la cadence guest |
| injection clavier/souris post-XInput | `divergent` | replay au seam XAM poll-exact ; injections désactivées |
| mode POSIX ReXGlue erroné | `divergent` | test open/read/write/flush/save obligatoire sur l'oracle Linux |
| race XAudio client | `divergent` | counters et test unregister/submit ; aucun drop dans une capture audio |
| SIGSEGV non réémis | `divergent` | watchdog + test crash synthétique avant qualification Linux |
| `mullhwucrash.cpp` généré | `divergent` pour le produit | maintenir l'audit generated/mappings, sans confiance fondée sur le nom du fichier |
| branche Linux + SDK tag | `documented-unmatched` au binaire | exiger manifeste de build/binary SHA et XEX exact |
| import ISO par `default.xex` seul | `divergent` | conserver import PAL v2 content-addressed et atomique |
| gameplay/release reNut | aucun `retail-qualified` | aucune lane M01 fermée |

## Validations reproductibles

- `git ls-remote`, HEAD/arbre, 199 commits, branches et 17 tags recoupés ;
- tag ReXGlue `nightly-20260809-f5c85215` résolu au commit/tree ci-dessus ;
- checkout propre Linux : échec CMake attendu et localisé au fichier généré
  absent ; aucun contenu retail requis ;
- deux AppImages et un ZIP hashés ; SquashFS extraits sans lancement ; manifestes
  des deux AppImages comparés byte à byte (neuf fichiers identiques, deux icônes
  différentes) ;
- `extract-xiso` embarqué recoupé après `strip` avec le blob public du commit
  `aa7c2dce96f0557f281a5b988fcadab1e6b50b25` ;
- binaires inspectés par `file`, `readelf`, `objdump`, `strings` et recherche de
  signatures, sans chargement dynamique ;
- comportement async, POSIX, audio et signal recoupé dans le SDK exact visé ;
- permaliens du rapport contrôlés ; `git diff --check` ciblé propre.

## Risques résiduels

- aucune identité US ne permet de reproduire le codegen reNut ;
- aucun reçu ne lie les releases au commit Linux, au SDK ou à un worktree
  propre ;
- les binaires n'ont pas été exécutés, donc les patches reNut ne sont pas
  qualifiés dynamiquement ;
- les deux PNG Windows n'ont aucune provenance publique ;
- aucun test, CTest ou workflow CI n'est présent dans la branche principale ou
  le snapshot Linux ;
- toutes les conclusions Xbox 360/AC6 restent soumises au XEX PAL canonique et
  à une observation retail ultérieure.

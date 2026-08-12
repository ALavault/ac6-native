# Cycle 1553 — audit Crazy Taxi, XenonRecomp vers ReXGlue

## Verdict

Le dépôt Crazy Taxi XBLA illustre surtout un **bring-up rapide qui devient
fail-open**. Son HEAD public est rendu jouable en faisant continuer 68 branches
de `switch` hors plage et en transformant les appels indirects non résolus en
retours zéro. Ce modèle est incompatible avec les gates AC6 : une progression
visible ne démontre ni contrôle-flow complet, ni ABI correcte, ni fidélité
retail.

Le projet confirme néanmoins trois pratiques utiles, uniquement au niveau
`provisional-rexglue` : conserver `skip_lr=false` pour instrumenter les
appelants, intercepter XAM au seam invité plutôt qu'au périphérique hôte, et
séparer l'initialisation du runtime, le chargement XEX, l'enregistrement des
entrées puis le lancement du thread invité. Notre replay PAL applique déjà ces
principes avec des contrats plus stricts.

Aucune sémantique Crazy Taxi ne devient `retail-qualified` pour AC6. Le SDK
ReXGlue utilisé n'est pas épinglé ni présent, les affirmations de timing ne
correspondent plus au HEAD, aucun test automatisé n'existe, et les outils
d'import ne vérifient ni l'intégrité STFS/XEX ni les bornes nécessaires.

## Provenance vérifiée

| Élément | Valeur au 12 août 2026 |
|---|---|
| dépôt | [`sp00nznet/ctxbla`](https://github.com/sp00nznet/ctxbla) |
| branche `main` | `23cce0a46cbc42bde0ecf6df80e568f83772f5ba` |
| arbre Git | `5cc5e2d8f1f5967e6089674f88e75a091bc87358` |
| date du HEAD | `2026-02-27T23:50:17-08:00` |
| historique public | 6 commits, aucun tag |
| patch VMX réduit | SHA-256 `5e2213e1d4af1e8212e048d90cd7d65b47b0277eaa81560413c5753bf456a6f0` |
| patch général | SHA-256 `27bb8cd924bce916bc030ef18e26ad7401142ec49a9ba82491d9533a830f4b6a` |

Le HEAD a été recoupé avec `git ls-remote`, puis le clone partiel a été rendu
complet. Il n'existe ni submodule, ni lockfile, ni commit du SDK ReXGlue ou de
XenonRecomp. Le
[`CMakeLists.txt`](https://github.com/sp00nznet/ctxbla/blob/23cce0a46cbc42bde0ecf6df80e568f83772f5ba/project/CMakeLists.txt#L9-L17)
cherche seulement une installation externe via `REXSDK`. Le README annonce
ReXGlue `v0.1.0`, le guide annonce `v0.2.0`, et `CLAUDE.md` décrit une jonction
locale vers le SDK d'un autre projet. Le binaire publié n'est donc pas
reproductible à partir d'une révision de runtime déterminée.

Le dépôt ne contient aucune licence racine. Les mentions de provenance dans
les outils ne constituent pas une licence du projet ou du million de lignes
C++ générées et commitées.

## Nature exacte de la « migration »

Deux configurations coexistent :

- [`crazytaxi.toml`](https://github.com/sp00nznet/ctxbla/blob/23cce0a46cbc42bde0ecf6df80e568f83772f5ba/config/crazytaxi.toml)
  produit un ancien corpus XenonRecomp, aujourd'hui ignoré par Git ;
- [`crazytaxi_rexglue.toml`](https://github.com/sp00nznet/ctxbla/blob/23cce0a46cbc42bde0ecf6df80e568f83772f5ba/config/crazytaxi_rexglue.toml)
  pilote le codegen ReXGlue actuel avec `skip_lr=false` et deux frontières de
  fonctions ajoutées manuellement.

La cible active compile 19 fichiers ReXGlue générés : 9 008 corps
`PPC_FUNC_IMPL`, 9 374 entrées de mapping et environ 1,3 million de lignes
ajoutées par le commit de bring-up. Le dépôt est donc une migration d'outil et
de runtime, pas un hybride où la sortie XenonRecomp resterait le produit
exécuté. Les patches XenonRecomp conservés sont une archive d'exploration, pas
la source du binaire HEAD.

Ce point est utile à notre taxonomie : une étiquette « migration depuis
XenonRecomp » ne permet pas d'attribuer les sémantiques exécutées à
XenonRecomp. Il faut identifier le générateur exact de chaque artefact.

## Contrôle-flow : le principal contre-exemple

Le fichier
[`ppc_detail.h`](https://github.com/sp00nznet/ctxbla/blob/23cce0a46cbc42bde0ecf6df80e568f83772f5ba/ppc/ppc_detail.h#L18-L90)
redéfinit deux frontières fondamentales :

1. `__builtin_trap()` devient un avertissement limité à dix occurrences, puis
   l'exécution continue après le `switch` ;
2. un appel indirect nul, hors plage, sans mapping ou avec thunk non résolu
   journalise quelques fois, force `r3=0`, puis continue.

Le corpus généré contient 68 défauts de `switch` ainsi neutralisés et 2 907
sites `PPC_CALL_INDIRECT_FUNC`. Les commentaires affirment que certains
indices hors plage sont « légitimes », mais aucun corpus ne les recense et
aucune cible attendue n'est prouvée. Continuer après le défaut n'émule pas une
destination PPC ; cela saute un effet inconnu.

Pour AC6, les invariants opposés sont retenus :

- toute cible indirecte exécutée doit être dans le catalogue qualifié ou
  produire une première divergence structurée ;
- un `switch` hors domaine est une erreur déterministe, jamais un no-op ;
- le nombre de sites non atteints ne vaut pas couverture ; il faut un census
  du replay M01 et la liste exacte des cibles observées ;
- aucune valeur de retour synthétique ne peut fermer JF/JV/JP/JG.

Le mécanisme de commit de pages invitées est également fail-open. Le
[`GuestPageCommitHandler`](https://github.com/sp00nznet/ctxbla/blob/23cce0a46cbc42bde0ecf6df80e568f83772f5ba/project/src/main.cpp#L154-L173)
transforme toute violation d'accès dans une plage hôte de 8 Gio en page
lecture-écriture nouvellement engagée, puis reprend l'instruction. Il ne
respecte ni le mapping, ni les permissions, ni l'état de commit Xbox 360. Une
adresse invitée invalide peut ainsi devenir de la mémoire zéro valide. Ce
patron est classé `divergent`.

## Instruction set et SIMD

Les deux patches revendiqués ne sont ni épinglés à un commit XenonRecomp, ni
accompagnés de tests. Le patch de 385 lignes est un sous-ensemble exact du
patch de 621 lignes. Ce dernier ajoute 95 labels de `case`, mais plusieurs sont
du contexte du diff ; les 23 instructions annoncées par le README ne sont donc
pas dérivables de ce simple compte.

Les implémentations montrent les risques d'une traduction directe par SIMD :

- les variantes saturantes utilisent `pack*`/`adds*` hôte sans mise à jour
  visible de `VSCR.SAT` ;
- `vrlh` émet un décalage droit de 16 lorsque le compte vaut zéro, cas qui
  dépend des règles du C++ et de la largeur du type promu ;
- les branches `bdzf`/`bdnzt` réduisent le champ BI à `CR.eq`, et le contexte
  existant dit déjà « assuming eq » pour `bdnzf` ;
- les opérations de comparaison record-form construisent CR6 depuis des
  masques SIMDe sans vecteurs de preuve ;
- le contexte du patch montre que le XenonRecomp ciblé traite déjà `sync`
  comme no-op, sans contrat mémoire ni test de publication inter-thread.

Le CMake impose `-msse4.1` sur hôte non-MSVC et le corpus généré contient plus
de 69 000 occurrences SIMDe. Cela répond à la question générale « le C++ peut
être traduit en SSE/AVX », mais ne prouve aucune équivalence Xenon. AC6 doit
continuer à écrire une sémantique C++ bornée et testée ; l'auto-vectorisation
ou SIMDe est ensuite un choix d'implémentation, jamais l'oracle.

Ces patches restent `documented-unmatched`. Aucun code ne doit être repris.

## Timing et cadence

Le
[`speed-fix.md`](https://github.com/sp00nznet/ctxbla/blob/23cce0a46cbc42bde0ecf6df80e568f83772f5ba/docs/speed-fix.md)
affirme que le HEAD remplace `__rdtsc()` et `VdSwap`. Ce n'est plus vrai dans
les sources suivies :

- `ppc_config.h` ne définit plus l'override `__rdtsc` ;
- `ppc_detail.h` dit explicitement que le SDK emploie le TSC hôte et laisse un
  `TODO` ;
- le stub local `VdSwap` à `QueryPerformanceCounter` a disparu au commit de
  codegen ;
- les 14 `mftb` générés appellent `PPC_QUERY_TIMEBASE()`, dont la définition
  appartient au SDK externe non épinglé.

Le statut « Timebase scaling done » est donc `documented-unmatched`. Même
l'ancien limiteur présentait une cadence hôte de 16 667 µs et remettait
`s_last` à l'heure courante : il ne démontrait ni vblank Xenon, ni cadence du
marqueur invité, ni rattrapage contrôlé.

Conséquence directe pour notre replay : aucun `source_hz` ne doit venir d'un
README, d'une constante de limiteur ou d'un compteur `PRESENT`. La cadence
doit être dérivée d'un census brut lié au binaire producteur, au marqueur
invité et à la fenêtre exacte.

## XAM, contrôleur et UI

Le pilote
[`keyboard_driver.cpp`](https://github.com/sp00nznet/ctxbla/blob/23cce0a46cbc42bde0ecf6df80e568f83772f5ba/project/src/keyboard_driver.cpp#L63-L193)
est un contre-exemple pour le replay :

- il ne sert que l'utilisateur 0 et ignore `flags` ;
- sous Windows, chaque `GetState` repolle `GetAsyncKeyState` et XInput hôte ;
- sous Linux, il retourne un état neutre avec succès ;
- le numéro de paquet ne change que lorsque le masque de boutons change, pas
  pour les triggers ou les quatre axes ;
- `GetCapabilities` annonce tous les contrôles, `GetKeystroke` retourne
  toujours vide et la vibration réussit sans effet ;
- son insertion en tête de chaîne peut donc masquer un pilote plus fidèle.

Il ne peut fournir ni entrée poll-exacte, ni preuve de hotplug, ni replay
cross-lane. Notre seam `XamInputGetState_entry` avant le périphérique reste
plus complet : utilisateur demandé, flags, pointeur invité, résultat, LR,
ordre global et octets logiques doivent tous être capturés.

Le dernier commit remplace `XamShowMessageBoxUI` par une macro dans le C++
généré et
[`ct_auto_accept_messagebox`](https://github.com/sp00nznet/ctxbla/blob/23cce0a46cbc42bde0ecf6df80e568f83772f5ba/project/src/stubs.cpp#L23-L72)
choisit toujours le bouton zéro. Il conserve une idée utile : borner
explicitement les notifications `XN_SYS_UI` autour d'une complétion synchrone
ou overlapped. Mais l'implémentation :

- ne valide ni `button_count`, ni l'index actif, ni les pointeurs invités ;
- lit directement le stack invité à `r1+0x54` ;
- capture `base` et l'adresse résultat dans une lambda différée sans preuve de
  durée de vie ;
- dépend d'une latence hôte du dispatcher SDK ;
- supprime un choix utilisateur et contourne le symbole kernel normal.

Elle est `divergent`, pas un générateur d'entrée. Pour AC6, la route boot→M01
doit être enregistrée par contrôleur puis rejouée au même seam, sans
auto-accepter une boîte inconnue.

## XMA, rendu et services runtime

Le dépôt ne contient pas le command processor, le décodeur XMA, la VFS ou les
services noyau ReXGlue exécutés. Le README affirme que le titre et les démos
ont du son mais que l'audio en jeu échoue sur le registre XMA `0x0601`. Sans
fork SDK, fixture, capture PCM ou test, cette observation est seulement un cas
négatif : boot + audio partiel ne qualifie pas XMA.

Le rendu D3D12 visible dans les captures reste celui du SDK externe. Le titre
active même `gpu_allow_invalid_fetch_constants=true` avant le lancement, sans
census des constantes concernées. Ce fallback est incompatible avec le rejet
déterministe des formats/états inconnus exigé par M01-B/JV.

Les stubs titre retournent zéro pour UI, réseau, caméra USB, statistiques,
I/O kernel et gestion d'objets. Leur réussite apparente ne documente ni
l'ABI, ni les effets secondaires, ni les erreurs attendues. Ils ne peuvent
servir d'implémentations provisoires à AC6 sans un appel réellement observé et
un contrat minimal spécifique.

## Import et contenu

L'extracteur STFS
[`extract_stfs.py`](https://github.com/sp00nznet/ctxbla/blob/23cce0a46cbc42bde0ecf6df80e568f83772f5ba/tools/extract_stfs.py#L34-L167)
ne convient pas au produit :

- `assert` valide le magic et disparaît sous `python -O` ;
- signatures et tables de hashes ne sont jamais vérifiées ;
- les chaînes de blocs sont remplacées par une progression séquentielle, même
  quand le fichier n'est pas marqué contigu ;
- un short read décrémente quand même `remaining` ;
- les noms ASCII et relations parent sont concaténés sans confinement du
  chemin de sortie ;
- les tailles de clusters incohérentes ne produisent qu'un avertissement ;
- les fichiers sont écrits directement et non atomiquement.

L'extracteur XEX
[`extract_pe.py`](https://github.com/sp00nznet/ctxbla/blob/23cce0a46cbc42bde0ecf6df80e568f83772f5ba/tools/extract_pe.py#L42-L256)
lit tout en mémoire, journalise la clé de fichier déchiffrée, ne valide ni la
signature XEX ni les SHA-1 de blocs, et manque de contrôles systématiques sur
les offsets/taille. Une erreur LZX produit une image entièrement zéro ; un
type de compression inconnu copie les octets bruts ; même une image sans
signature PE reconnue est écrite et la fonction retourne succès.

Ces outils sont des cas de test négatifs utiles à notre import v2. Le produit
AC6 conserve son cache content-addressed, ses bornes strictes, ses écritures
atomiques et son refus des octets non authentifiés ou tronqués.

## Tests, build et packaging

`crazytaxi_test` n'est pas un test automatisé : il charge le XEX retail puis
attend la fin du module. Le CMake n'appelle ni `enable_testing()` ni
`add_test()`. Il n'existe aucune suite unitaire, CI, sanitizer, audit de
provenance, validation Vulkan/D3D12 ou contrôle de paquet.

Le CMake possède une branche POSIX, mais l'unique preset est Windows et la
couche titre utilise largement Win32/SEH. Aucun build Linux public n'est
qualifié. Les settings annoncent TOML mais `Load()` utilise toujours les
valeurs par défaut ; `Save()` tronque directement un fichier relatif et le
défaut `unlock_all_content=true` n'est pas relié au contrôle de licence.

## Contrats à retenir pour AC6 M01

| Observation Crazy Taxi | Classe | Action AC6 |
|---|---|---|
| `skip_lr=false` dans le codegen actif | `provisional-rexglue` | conserver LR pour le replay oracle PAL |
| init runtime → load XEX → input → launch | `provisional-rexglue` | garder un armement avant le premier poll invité |
| notifications UI encadrant overlapped | `provisional-rexglue` | tester seulement après ABI/événement PAL qualifié |
| trap de switch transformé en no-op | `divergent` | test fatal avec adresse, index et premier écart |
| appel indirect manquant → `r3=0` | `divergent` | aucun fallback ; catalogue exécuté obligatoire |
| commit automatique sur toute faute invitée | `divergent` | préserver mapping et permissions explicites |
| input repollé à chaque appel hôte | `divergent` | replay au seam XAM, ordonné par poll |
| cadence/timebase annoncées mais non présentes | `documented-unmatched` | census brut scellé obligatoire |
| XMA/rendu/services du SDK absent | `documented-unmatched` | ne rien qualifier par transitivité |
| extracteurs STFS/XEX permissifs | `divergent` | corpus négatif pour lecteurs fail-closed |

## Conclusion M01

Crazy Taxi n'apporte aucun port de code à effectuer. Il apporte une garde
importante contre une fausse accélération : **ne pas confondre jeu visible et
contrôle-flow qualifié**. Avant de promouvoir ReXGlue comme oracle provisoire
pour M01, notre stack doit compter les défauts de `switch`, appels indirects,
stubs et fallbacks réellement atteints et refuser toute occurrence non
explicitement autorisée.

Le plus petit lot concret issu de cet audit est donc un audit de trace oracle
PAL qui exige, sur la fenêtre replay retenue : zéro branche inconnue neutralisée,
zéro cible indirecte absente, zéro page invitée créée par faute et zéro retour
synthétique d'un stub non qualifié. Cela accélère le bring-up sans transformer
les approximations ReXGlue en preuves retail.

## Validation de l'audit

- `git fetch --unshallow --tags` puis `git ls-remote` ont confirmé le HEAD et
  l'absence de tags publics ;
- les six commits et l'arbre Git ont été relus dans un clone propre ;
- les nombres 9 008, 9 374, 68, 2 907 et 14 ont été recalculés depuis les
  sources suivies ;
- les deux patches ont été hashés et comparés structurellement ;
- aucun XEX, container STFS, fichier audio, capture runtime ou autre octet
  retail n'a été lu ;
- aucun code généré ou dépôt externe n'a été modifié.

# Cycle 1560 — audit du runtime historique Skate 2

## Verdict

Le dépôt public Skate 2 est un **squelette de bring-up XenonRecomp autonome,
Windows/D3D9 et incomplet**, pas une référence d'exactitude Xbox 360. Il expose
des seams utiles à comparer — arène invitée de 4 Gio, table de fonctions,
pont ABI typé, hooks kernel/XAM/D3D et structures big-endian — mais le produit
public ne peut pas être reconstruit : le générateur XenonRecomp n'est pas
épinglé, le corpus `src/ppc` est absent, deux shaders hôte requis sont absents,
et aucun build, test ou CI n'est publié.

Aucune sémantique n'est `retail-qualified`, ni pour Skate 2 ni, a fortiori,
pour AC6 PAL Mission 01. Le dépôt ne publie aucun SHA-256 de XEX, région/TU,
trace d'exécution qualifiée ou résultat attendu. Les adresses Skate 2, noms
`sub_*`, tables de sauts et observations du runtime ne doivent jamais être
transférés à AC6 par transitivité.

Les apports M01 se limitent à des **contre-exemples et patrons
`documented-unmatched`** : conserver le seam guest/host explicitement typé, tester les
64 bits et la pile de l'ABI, rendre les erreurs de codegen fatales, séparer
VFS data/save et refuser tout renderer ou XMA qui progresse par no-op. Le code
Skate 2 n'est pas candidat au port direct, notamment faute de licence racine.

## Taxonomie appliquée

| État | Conclusion dans cet audit |
|---|---|
| `provisional-rexglue` | **Inapplicable ici** : Skate 2 n'utilise pas ReXGlue. |
| `retail-qualified` | **Aucun élément.** Aucun XEX n'est qualifié par SHA-256 et aucune exécution reproductible n'est fournie. |
| `divergent` | Comportement dont la source montre un écart Xenon/Xenos/XAM/XMA, un no-op, un fallback hôte ou une ABI incorrecte. |
| `documented-unmatched` | Intention, nom, commentaire, commit ou configuration sans artefact exécutable/test public correspondant. |

Cette taxonomie ne contient pas de catégorie implicite « probablement
correct ». Un patron `documented-unmatched` ne ferme aucune gate AC6.

## Provenance Git vérifiée

| Élément | Valeur au 12 août 2026 |
|---|---|
| dépôt | [`Skate-2-Team/sk82-recomp`](https://github.com/Skate-2-Team/sk82-recomp) |
| branche publique | `main` uniquement |
| HEAD | [`63ef2e191a493348063c55001838b9c7d86100fe`](https://github.com/Skate-2-Team/sk82-recomp/commit/63ef2e191a493348063c55001838b9c7d86100fe) |
| arbre Git | `7a6d4d617330f1718d6b6957cec52903afd91a60` |
| parent | `5cb21466ba2e1f27a439f0debf03a851b0883f97` |
| date HEAD | `2026-07-29T18:17:45+10:00` |
| historique | 26 commits, aucun tag |
| dernier changement source | `5cb21466ba2e1f27a439f0debf03a851b0883f97`, 21 mai 2025, « XMA*() function implementation + ffmpeg linkage » |

Le HEAD ne modifie que deux lignes du
[`README`](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/README.md#L1-L13)
pour déclarer le projet abandonné. `git ls-remote` recoupe le même SHA pour
`HEAD` et `refs/heads/main` ; aucun tag ou autre branche publique n'a été
retourné.

Les trois gitlinks et leurs arbres ont été récupérés directement par leur SHA,
avec filtre de blobs :

| Sous-module | Gitlink | Arbre vérifié | Licence amont au pin |
|---|---|---|---|
| SDL | [`210b317d8dc638407505390a032a11c54b48d789`](https://github.com/libsdl-org/SDL/commit/210b317d8dc638407505390a032a11c54b48d789) | `1e7c13e35ab1c8c4b280c7db3822d69d23d477e4` | [zlib](https://github.com/libsdl-org/SDL/blob/210b317d8dc638407505390a032a11c54b48d789/LICENSE.txt) |
| unordered_dense | [`73f3cbb237e84d483afafc743f1f14ec53e12314`](https://github.com/martinus/unordered_dense/commit/73f3cbb237e84d483afafc743f1f14ec53e12314), tag `v4.5.0` | `7a8551555c2ad4f01756cc5e461a049d446dfe68` | [MIT](https://github.com/martinus/unordered_dense/blob/73f3cbb237e84d483afafc743f1f14ec53e12314/LICENSE) |
| xxHash | [`953a09abc39096da9e216b6eb0002c681cdc1199`](https://github.com/Cyan4973/xxHash/commit/953a09abc39096da9e216b6eb0002c681cdc1199) | `8b766b22b3682b90dd84336874e6be616125f9e8` | [BSD-2-Clause](https://github.com/Cyan4973/xxHash/blob/953a09abc39096da9e216b6eb0002c681cdc1199/LICENSE) |

Les URLs sont bien celles du
[`.gitmodules`](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/.gitmodules#L1-L9)
et aucun de ces arbres ne déclare de sous-module imbriqué. Cela qualifie les
dépendances elles-mêmes, pas la licence du runtime Skate 2.

## Architecture XenonRecomp et runtime autonome

La configuration
[`recomp.toml`](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/recomp_config/recomp.toml#L1-L32)
vise `default.xex`, un répertoire `ppc`, 902 tables de sauts, 105 frontières de
fonctions ajoutées, les helpers GPR/FPR/VMX/VMX128 et `setjmp`/`longjmp`. Tous
les flags d'optimisation sont désactivés, dont `skip_lr=false` et
`skip_msr=false`. La prudence de cette configuration est
`documented-unmatched` comme
checklist ; ses adresses sont propres à un binaire Skate 2 non qualifié.

Le runtime public suit cette séquence :

1. réservation Windows d'une arène invitée de 4 Gio, préférée à
   `0x100000000`, puis remplissage d'une table adresse invitée/fonction hôte
   ([`memory.cpp`, lignes 6–29](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/kernel/memory.cpp#L6-L29)) ;
2. conversion adresse hôte/invitée par différence avec la base et lookup des
   appels recompilés
   ([lignes 32–60](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/kernel/memory.cpp#L32-L60)) ;
3. chargement de `game/default.xex`, copie de l'image puis lancement de son
   entry point dans un `std::thread`
   ([`main.cpp`, lignes 19–39](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/main.cpp#L19-L39)) ;
4. allocation PCR/TLS/TEB/stack invité, initialisation de `r1`, `r13` et du
   FPSCR
   ([`guest_thread.cpp`, lignes 9–38](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/cpu/guest_thread.cpp#L9-L38)),
   puis dispatch par adresse
   ([lignes 72–83](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/cpu/guest_thread.cpp#L72-L83)).

Cette décomposition seam mémoire → loader → mapping → thread est
`documented-unmatched`. Elle rend visibles les responsabilités qui doivent rester
séparées dans AC6. Elle n'est toutefois pas un runtime autonome reproductible :

- le dépôt ne contient aucun submodule/pin XenonRecomp ou XenonAnalyse ;
- `src/ppc` ne contient qu'un `.gitignore`, alors que le
  [`CMakeLists.txt`](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/CMakeLists.txt#L1-L14)
  exige `ppc_recomp.*.cpp`, `ppc_recomp_shared.h`, `ppc_context.h` et
  [`ppc_func_mapping.cpp`](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/CMakeLists.txt#L66-L76) ;
- aucun mode d'emploi ne relie le répertoire de sortie `ppc` de la config au
  `src/ppc` compilé ;
- 350 hooks sont enregistrés, mais 191 corps journalisent explicitement
  `Log::Stub`; le nombre de noms couverts ne mesure donc pas la sémantique.

Le loader est `divergent` pour les entrées non triviales. Il ne valide ni
magic, bornes, digest, chiffrement, imports ou TLS. Il traite seulement les
compressions `NONE` et `BASIC`; `NORMAL` et `DELTA` tombent jusqu'au message
« XEX loaded » et renvoient succès sans image copiée
([`loader.cpp`, lignes 18–69](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/loader.cpp#L18-L69)).
Ce fail-open est un contre-modèle pour le chargement AC6.

## ABI, endian et VMX/VMX128

La forme à retenir comme patron `documented-unmatched` est explicite : les pointeurs
invités sont des `be<uint32_t>` traduits dans l'arène hôte
([`xbox.h`, lignes 113–159](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/kernel/xbox.h#L113-L159)),
et les structures sensibles utilisent des wrappers big-endian et des
`static_assert` de taille/offset. Ce sont des gardes de forme, pas une preuve
de valeurs ou de layout AC6.

Le pont ABI est en revanche `divergent` pour les registres 64 bits :

- tous les arguments GPR `r3` à `r10` sont lus par leur vue `.u32`, même si le
  type hôte demandé est `uint64_t`, et chaque argument stack est lu comme un
  seul `be<uint32_t>`
  ([`function.h`, lignes 45–75](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/kernel/function.h#L45-L75)) ;
- les arguments flottants au-delà de `f13` retournent zéro
  ([lignes 77–112](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/kernel/function.h#L77-L112))
  et les écritures d'arguments GPR/FP hors registres déclenchent un assert
  ([lignes 115–200](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/kernel/function.h#L115-L200)) ;
- le wrapper de retour sait écrire `r3.u64`, ce qui ne répare pas la
  troncature en entrée
  ([lignes 343–378](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/kernel/function.h#L343-L378)).

La présence des adresses `restvmx_14`, `savevmx_14`, `restvmx_64` et
`savevmx_64` dans la config est `documented-unmatched`. Le contexte PPC et les
implémentations d'instructions ne sont pas publiés. Le seul artefact de
codegen, [`utils/xenonlog.txt`](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/utils/xenonlog.txt#L1-L18),
atteint 100 % tout en recensant 482 sites d'instructions non reconnues de 16
types et neuf comparaisons record-form sans comparaison générée. Les lacunes
dominantes sont `vcfpuxws128` (145), `vaddsws` (125), `vpkuwus128` (36) et
`vsel128` (34) ; elles persistent jusque dans les dernières lignes
([lignes 501–522](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/utils/xenonlog.txt#L501-L522)).
Le log n'indique ni version d'outil, commande, XEX SHA-256, ni politique
d'émission aux sites inconnus. Il est une garde négative `divergent`, pas un
test VMX transférable.

## D3D9, Xenos et renderer

Le backend est une traduction **haut niveau propre au jeu**. SDL crée une
fenêtre, extrait son `HWND`, puis ouvre un périphérique Direct3D 9
([`video.cpp`, lignes 8–58](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/graphics/video.cpp#L8-L58)).
Les hooks `sub_*` interceptent création de device, draws, textures et états ;
ils ne consomment pas le ring PM4 et n'utilisent ni XenonRecomp/XenosRecomp
épinglé ni shader translator public.

La séparation « structures invitées → conversion → API hôte » et les
`static_assert` du `GuestDevice` sont `documented-unmatched` comme organisation
([`d3d_context.h`, lignes 1818–1846](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/graphics/d3d_context.h#L1818-L1846)).
Le comportement renderer est `divergent` :

- les APIs de ring, command buffer, EDRAM et interrupt GPU sont des stubs
  ([`xgpu.cpp`, lignes 17–112](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/kernel/hooks/xgpu.cpp#L17-L112)) ;
- `BeginIndexedVertices` renvoie succès sans buffer, les deux draws indexés
  sont vides et les deux clears sont no-op
  ([`video_hooks.cpp`, lignes 343–395](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/graphics/video_hooks.cpp#L343-L395)) ;
- resolve, surfaces, tiling, predication, synchronisation de présentation et
  constantes pending sont explicitement stubés
  ([lignes 783–795](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/graphics/video_hooks.cpp#L783-L795)) ;
- le draw immédiat échange chaque mot de 32 bits, ne charge que huit constantes
  VS et huit PS, puis appelle `DrawPrimitiveUP`
  ([lignes 225–301](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/graphics/video_hooks.cpp#L225-L301)) ;
- les formats EDRAM retournent `D3DFMT_UNKNOWN`
  ([`d3d_context.h`, lignes 888–905](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/graphics/d3d_context.h#L888-L905)) ;
- le chemin texture spécial n'accepte que DXT5, un mip, et interrompt sur cube;
  le chemin générique abandonne tous les formats sauf L8
  ([`video_hooks.cpp`, lignes 400–486](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/graphics/video_hooks.cpp#L400-L486)).

Le support shader est `documented-unmatched`. `PrecompileShaders` exige
`shaders/vp6.fx` et `shaders/passthrough.fx`, absents de l'arbre, tandis que le
hook vertex shader est vide
([`shaders.cpp`, lignes 72–110](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/graphics/shaders.cpp#L72-L110)).
Le seul chargement dynamique cherche un `.fpo` hôte dérivé d'un chemin UPDB et
ne couvre que les pixel shaders file-backed
([lignes 123–175](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/graphics/shaders.cpp#L123-L175)).
Rien ici ne qualifie PM4, Xenos, EDRAM, memexport ou timing AC6.

## XMA, FFmpeg et sortie audio

Le commit final de code annonce une implémentation XMA et une liaison FFmpeg,
mais la partie décodage est `documented-unmatched`. Le dépôt lie
`avcodec.lib`, `avutil.lib` et `avformat.lib`; dans le code, les seules fonctions
FFmpeg appelées sont les trois destructeurs `avcodec_free_context`,
`av_frame_free` et `av_packet_free`. Il n'existe aucun find/open/send/receive
de codec, parseur de paquet XMA, production PCM ou replay.

Le modèle de contexte est `divergent` : un `struct` bitfield natif décrit 16
DWORD supposés matériels, puis ajoute des pointeurs hôte FFmpeg et des booléens
dans la même structure packée
([`xma.h`, lignes 15–83](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/kernel/hooks/xma.h#L15-L83)).
L'ordre des bitfields dépend du compilateur/hôte et n'est pas un encodage
big-endian Xenon. Les setters ne font que copier pointeurs, compteurs et flags;
les offsets de lecture/écriture ne progressent jamais
([`xma.cpp`, lignes 34–113](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/kernel/hooks/xma.cpp#L34-L113)).
En outre, `XMAReleaseContext` libère l'objet sans retirer son pointeur de
`g_contexts`, laissant le handle référençable après libération.

La couche XAudio n'émet aucun son : une boucle hôte appelle le callback invité
toutes les 1 ms
([`audio_hooks.cpp`, lignes 21–35](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/audio/audio_hooks.cpp#L21-L35)),
mais `XAudioSubmitRenderDriverFrame` renvoie immédiatement zéro
([lignes 65–95](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/audio/audio_hooks.cpp#L65-L95)).
`SdlAudio::PutBuffer` n'a aucun appelant. Il n'existe ni ring PCM, cadence
sample-exacte, fallback `SDL_AUDIODRIVER=dummy`, ni test headless. Ce chemin ne
doit pas remplacer le boundary XMA explicite d'AC6.

Le bundle FFmpeg suivi contient 186 fichiers. Ses 43 fichiers sous `bin/` et
`lib/` occupent 168 819 291 octets non compressés et ciblent Windows. Le header
[`ffversion.h`](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/external/ffmpeg/include/libavutil/ffversion.h#L1-L5)
annonce `N-119209-gb02985b12c-20250411`, mais aucun manifeste de build,
configuration, source correspondante, `COPYING` ou notice binaire n'est livré.
Cette provenance partielle ne rend ni le build ni la licence du bundle
reproductibles.

## Input et replay

Le seam direct `XamInputGetState` est `documented-unmatched` comme emplacement de hook,
pas comme comportement. L'implémentation lit l'état clavier SDL au moment du
poll, ignore `userIndex` et `flags`, remet toute la structure à zéro, puis
renvoie toujours succès
([`input_hooks.cpp`, lignes 46–110](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/input/input_hooks.cpp#L46-L110)).

Elle est `divergent` pour un replay M01 :

- `dwPacketNumber` reste toujours zéro ;
- stick droit et D-pad utilisent `SDL_SCANCODE_UNKNOWN`, donc aucun binding ;
- aucun gamepad SDL, deadzone, hotplug, perte de focus ou vibration n'est
  implémenté ;
- aucune timeline, sérialisation, seed, tick invité ou mode record/replay
  n'existe.

Un `MainLoopHook` pompe les événements SDL
([`video_hooks.cpp`, lignes 5–11](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/graphics/video_hooks.cpp#L5-L11)),
mais aucun appel ou mapping n'est visible hors du corpus PPC absent : sa
reachability publique est `documented-unmatched`. Le replay poll-exact AC6 ne
peut tirer aucune équivalence de ce polling hôte.

## VFS, contenu et saves

La VFS est un contre-exemple `divergent`. `ResolvePath` supprime aveuglément
les trois premiers caractères et préfixe `game\\`, sans table de mounts,
normalisation, contrôle de traversal, casse, type de contenu ou racine save
([`hooks.h`, lignes 28–40](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/kernel/hooks/hooks.h#L28-L40)).
`CreateFileA` ouvre ensuite ce chemin directement avec `std::fstream`
([`xfilesystem.cpp`, lignes 8–52](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/kernel/hooks/xfilesystem.cpp#L8-L52)).

Les limites certaines sont :

- les écritures overlapped sont interdites par assert et le compte écrit vient
  de `gcount()` après une écriture
  ([lignes 204–216](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/kernel/hooks/xfilesystem.cpp#L204-L216)) ;
- la création de répertoire ne reconnaît que `d:\\` et appelle directement
  `CreateDirectoryA`
  ([lignes 218–239](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/kernel/hooks/xfilesystem.cpp#L218-L239)) ;
- l'énumération passe par une hash-map, donc sans ordre stable, et inverse les
  moitiés low/high de la taille
  ([`hooks.h`, lignes 406–447](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/kernel/hooks/hooks.h#L406-L447)) ;
- les primitives `NtCreateFile`, `NtReadFile`, `NtWriteFile`, liens symboliques
  et flush restent des stubs
  ([`xfilesystem.cpp`, lignes 285–359](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/kernel/hooks/xfilesystem.cpp#L285-L359)) ;
- `XamContentCreateEx`, licence et enumeration sont des stubs
  ([`xam.cpp`, lignes 30–69](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/kernel/hooks/xam.cpp#L30-L69)) ;
  profile read ne fait que zéro-remplir un buffer et profile write est un stub
  ([lignes 219–243](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/kernel/hooks/xam.cpp#L219-L243)).

Il n'existe ni STFS/XContent vérifié, ni atomic save, ni séparation data/save,
ni politique fail-closed. Aucun de ces chemins ne convient aux conteneurs ou
saves AC6.

## Build Linux, tests et reproductibilité

Le build Linux est `documented-unmatched` et statiquement impossible au HEAD :

- le seul script est
  [`build.bat`](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/build.bat#L1-L13),
  qui choisit MinGW/Clang sous Windows ;
- la cible lie `d3d9`, `d3dcompiler` et des `.lib` FFmpeg Windows
  ([`CMakeLists.txt`, lignes 78–93](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/CMakeLists.txt#L78-L93)) ;
- les sources utilisent `windows.h`, `VirtualAlloc`, `HWND`, Direct3D 9,
  `CreateDirectoryA`, `GetTickCount`, `ExitThread` et SEH ;
- [`mutex.h`](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/cpu/mutex.h#L1-L30)
  ne définit même `Mutex` que sous `_WIN32`, alors que le runtime l'instancie
  sans garde ;
- les sources PPC générées et les shaders nécessaires manquent avant même la
  phase de link.

L'arbre ne contient aucun `tests/`, CTest, framework de test, workflow
`.github`, lockfile, release tag ou résultat de build. `xenonlog.txt` est un
diagnostic de génération non qualifié, pas un oracle. Aucun build n'a été tenté
dans cet audit : atteindre la cible requerrait de générer le corpus depuis un
XEX retail non qualifié, explicitement hors périmètre.

## Licence et octets retail

Le dépôt n'a ni `LICENSE`, ni `COPYING`, ni `NOTICE`. Les commentaires
attribuent plusieurs fichiers à Unleashed Recompiled avec un lien mutable vers
`main`, par exemple
[`function.h`](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/kernel/function.h#L9-L12)
et
[`xfilesystem.cpp`](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/src/kernel/hooks/xfilesystem.cpp#L3-L6),
mais sans commit amont ni notice de licence reproduite. Le `README` disclaimer
n'est pas une licence. Les licences des sous-modules et l'en-tête MIT du
[`o1heap.c`](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/external/o1heap/o1heap.c#L1-L15)
ne couvrent pas le runtime racine. Le code ne doit donc pas être copié dans
AC6 sans clarification de droits et traçage source par source.

L'arbre de 256 entrées ne suit aucun `.xex`, `.xexp`, ISO, PAC, STFS ou
répertoire `game/`/`shaders/`. Les plus gros blobs sont les binaires FFmpeg ;
le `README` exige que l'utilisateur fournisse sa copie du jeu
([lignes 19–21](https://github.com/Skate-2-Team/sk82-recomp/blob/63ef2e191a493348063c55001838b9c7d86100fe/README.md#L19-L21)).
Les tables de sauts et le log restent des artefacts dérivés non rattachés à un
SHA retail. Aucun octet retail n'a été téléchargé, ouvert ou exécuté pendant
cet audit.

## Décision AC6 Mission 01

| Domaine | État | Décision |
|---|---|---|
| arène 4 Gio, pointeurs 32 bits, seam hook | `documented-unmatched` | Garder comme comparatif d'architecture ; requalifier chaque invariant sur AC6 PAL. |
| ABI GPR/stack/FP | `divergent` | Ajouter/maintenir des tests 64 bits, >8 GPR et >13 FP ; ne pas reprendre le pont. |
| VMX/VMX128 | `divergent` + `documented-unmatched` | Transformer les 16 familles du log en idées de fixtures génériques seulement ; aucune adresse/opcode Skate 2 ne qualifie AC6. |
| D3D9/Xenos | `divergent` | Rejeter comme backend ou oracle PM4/EDRAM ; conserver la frontière renderer explicite. |
| XMA/FFmpeg | `divergent` + `documented-unmatched` | Aucun ring, decode ou PCM réutilisable ; conserver le chemin AC6 borné et testé. |
| input/replay | `divergent` | Aucun apport au replay poll-exact M01. |
| VFS/saves | `divergent` | Utiliser comme liste de gardes négatives : mounts, traversal, ordre, atomicité, data/save. |
| Linux/build/tests | `documented-unmatched` | Aucun pipeline ou test transférable. |
| sémantique retail | `retail-qualified` | Ensemble vide ; aucune gate M01 fermée. |

## Validation reproductible

Audit effectué sur un clone `--filter=blob:none --no-checkout`, avec inspection
de l'arbre avant checkout sélectif des seuls petits fichiers source utiles.
Les contrôles principaux étaient :

```text
git ls-remote https://github.com/Skate-2-Team/sk82-recomp.git HEAD refs/heads/main 'refs/tags/*'
git rev-parse HEAD^{commit} HEAD^{tree}
git rev-list --count HEAD
git ls-tree -r -l HEAD
git diff-tree --no-commit-id --name-status -r HEAD
git grep -E '^[[:space:]]*GUEST_FUNCTION_HOOK\(' HEAD -- src
git grep -E '^[[:space:]]*Log::Stub' HEAD -- src
git show HEAD:utils/xenonlog.txt | sha256sum
```

Le log public vaut SHA-256
`bd8f02f717794a926580d1b653051bfb19d8a23adc326958f193a1a582b5102d`.
Les gitlinks ont chacun été validés par `git fetch --filter=blob:none --depth=1
<url> <sha>`, puis `git show -s --format='%H %T' FETCH_HEAD`. Les permaliens du
présent rapport ont répondu HTTP 200. `git diff --check` ciblé est propre ; le
contrôle `--no-index --check` du nouveau fichier ne produit aucun diagnostic
d'espace blanc.

Les seules interprétations génériques Xenon/Xenos ont été recoupées avec le
catalogue local `architecture-v1` : snapshots XenonRecomp
`b72d34f82f796e4c84f0f622fad8fb720b64607b842de31b70596d35157a68ff`,
XenosRecomp
`56a7c5074c166377554822b2812830d4f889844c39c10ae89ed5998b29a8f5e1`
et contexte PPC Xenia
`9acd682d4ba3cb6261dfd6da5613504e68b8d4643320a630436bfdcda76fd3f4`.
Leurs SHA-256 correspondent au catalogue ; celui-ci est documentaire et n'a
été utilisé comme preuve retail ni pour Skate 2 ni pour AC6.

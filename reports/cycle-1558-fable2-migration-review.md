# Cycle 1558 — audit Fable II, XenonRecomp vers ReXGlue

## Verdict

Le HEAD public Fable II épinglé n'est pas une migration sémantique vérifiable
de l'ancien runtime XenonRecomp. C'est un **reset vers un shell ReXGlue** : les
1096 entrées de l'arbre `legacy-main`, ses 26 gitlinks et ses 240 fichiers C++
PPC générés ont été remplacés par un arbre de 14 fichiers qui dépend d'un SDK
externe non versionné. Les trois commits suivants ajoutent une table de
fonctions, un `dlmalloc`, trois hooks et un lifecycle hôte, sans reçu reliant
ces artefacts à un XEX précis.

Aucune observation Fable II n'est `retail-qualified`, pour Fable II comme pour
AC6 PAL Mission 01. Les meilleurs apports sont des gardes négatives :

- épingler le SDK et l'identité du producteur avant le codegen ;
- normaliser et valider les clés d'adresses avant insertion ;
- isoler le code titre des fichiers que `rexglue migrate` écrase ;
- distinguer heap virtuel, mémoire physique et ressources Xenos ;
- séparer les racines retail en lecture seule des données utilisateur ;
- rendre chaque stub, backend et étape de teardown observable et testable.

Le seul patron directement conservable, classé `provisional-rexglue`, est
l'ordre général `Setup` → heap titre → chargement XEX → fenêtre → lancement
invité, suivi de `TerminateTitle` et d'un `join`. Il faut encore lui ajouter un
teardown du heap, une attente bornée et des tests de panne/restart.

## Taxonomie appliquée

| Classe | Sens dans cet audit |
|---|---|
| `retail-qualified` | comportement exécuté et comparé à un binaire retail identifié ; **aucun cas ici** |
| `provisional-rexglue` | forme de code ou de lifecycle inspectable, utile comme hypothèse d'architecture seulement |
| `divergent` | branche qui remplace explicitement un effet Xbox 360 par un autre effet ou perd de l'état |
| `documented-unmatched` | affirmation, nom, configuration ou service délégué sans artefact exécutable et pin permettant de la vérifier |

Une classe ne se propage jamais entre jeux. Même un comportement
`retail-qualified` pour Fable II resterait à requalifier sur le XEX PAL AC6,
le projet Ghidra canonique, le seam et la fenêtre M01 concernés.

## Provenance Git vérifiée

| Élément | Valeur au 12 août 2026 |
|---|---|
| dépôt | [`Fable2Recomp/Fable2Recomp`](https://github.com/Fable2Recomp/Fable2Recomp) |
| HEAD / `main` | [`1e25911172f8e30458099eda96a1ad7b8992ed60`](https://github.com/Fable2Recomp/Fable2Recomp/commit/1e25911172f8e30458099eda96a1ad7b8992ed60) |
| arbre | `d422a07a194b4b78602cc6cea451cbe19d75480e` |
| parent | `f37d819989f4d57d077b68cf1ab4e928b8a68fda` |
| auteur / committer | Tom / Ryan Fisher, `2026-03-05T00:02:25Z` |
| signature | aucun champ `gpgsig`; GitHub indique `unsigned` |
| historique `main` | 82 commits, aucun tag, aucune release, aucun workflow CI |
| autres têtes | `legacy-main` `9890500e5b0bc4ef17c7d4834e167fdaee483487`; `Building` `c252b1f2496fd5f6a7d77f3f4add5c84a1801448` |
| arbre HEAD | 14 fichiers, 328 660 octets de blobs, aucun gitlink, aucun `.gitmodules` |
| arbre `legacy-main` | 1096 fichiers, 334 076 907 octets de blobs, 26 gitlinks |

`git ls-remote`, `git cat-file -p` et `git ls-tree -r -l` donnent les mêmes
objets. Les arbres des branches sont respectivement
`47685925a6958c44baa2656813f2ccda766fcccd` pour `legacy-main` et
`16b3b090c29ce7d77dfaac330f9ef11fb496a52d` pour `Building`. Le HEAD ne contient
que le CMake, la configuration, le shell, le heap, les hooks et `dlmalloc` ; il
ne contient ni code recompilé, ni SDK, ni shader, ni XEX.

Les 26 gitlinks historiques ont été relus dans l'arbre `legacy-main` et
recoupés avec son
[`.gitmodules`](https://github.com/Fable2Recomp/Fable2Recomp/blob/9890500e5b0bc4ef17c7d4834e167fdaee483487/.gitmodules).
L'inventaire canonique `SHA chemin`, trié comme `git ls-tree -r`, a le SHA-256
`c5bb2cc7d91a48584888c9d1b14ba0c3cc1b8bfc1c038722786d1dd7bb5c94c6` :

```text
da8b81f7a71a08b523c1f66ed825124c81e5996e extern/D3D12MA
8ae962c90457b28e058e5611383a21c3e315aab7 extern/SDL3
b3a6fa8b5ad183f0a1bad02527d89a00c3c90106 extern/SDL_mixer
539c0a8d8e3733c9f25ea9a184c85c77504f1653 extern/VMA
e2e53a724677f6eba8ff0ce1ccb64ee321785cbd extern/Vulkan-Headers
73f3cbb237e84d483afafc743f1f14ec53e12314 extern/ankerl-unordered_dense
0748352e9791cd895ebda1c2ae0041e2224e377b extern/ddspp
64db979e38ec644b1798e41610b28c8d2c8a2739 extern/fmt
2d4c4b4dd31fde06cfffad7915c2b3006402322f extern/glm
be4ee7d0e3bfd151bfda7b3a8e03f8c49c55ed7b extern/glslang
faa03031b4cdf34fe9174c4e73dd769a5b41fda5 extern/imgui
3da8bd34299965d3b0ab124df743fe3e076fa222 extern/implot
305907723a4e7ab2018e58040059ffb5e77db837 extern/libmspack
f7eb7efdad8d8b98aba3f247da8d623b8d4a008a extern/msdf-atlas-gen
a1a401062819beb8c3da84518ab1fe7de88632db extern/nativefiledialog-extended
a5a8caa1951b3f893a08f63cab2dc877087dc05b extern/shaderc
2af3dce9b2481b6b32139b1022cdfc02a633c898 extern/simde
48bcf39a661a13be22666ac64db8a7f886f2637e extern/spdlog
c9aad99f9276817f18f72a4696239237c83cb775 extern/spirv-headers
01021466b5e71deaac9054f56082566c782bfd51 extern/spirv-tools
802cd454f25469d3123e678af41364153c132c2a extern/stb
23856752fbd139da0b8ca6e471a13d5bcc99a08d extern/tiny-AES-c
9148bdf719e997d1f474be6bcc7943881046dba1 extern/tinyxml2
8eb4012353d80696e624c0685cbd0baa69772a46 extern/tomlplusplus
43c29e655cb8117fd9cfb65ad9cefe2d40965102 extern/volk
1ec193eacfeae3e0da497373d41ccd850ebec9e5 extern/xxHash
```

Ces pins documentent seulement ce qui a été supprimé. Aucun n'appartient à la
fermeture de dépendances du HEAD, et aucun ne fournit un pin ReXGlue ou
XenonRecomp.

Le plus proche SDK public dans le temps est la release ReXGlue
[`v0.2.3`](https://github.com/rexglue/rexglue-sdk/releases/tag/v0.2.3), publiée
à `2026-03-04T21:12:17Z`, commit
[`88a8034347556cb6da79b92109847220cebfb080`](https://github.com/rexglue/rexglue-sdk/commit/88a8034347556cb6da79b92109847220cebfb080),
arbre `39c2ddc4b4d0f5422f27ddfaba4bb507141e8ccb`. Le HEAD Fable arrive 2 h 50
plus tard et sa forme correspond à cette génération du SDK. C'est seulement
un **candidat temporel** : aucun fichier Fable ne le référence.

Au jour de l'audit, la release ReXGlue
[`v0.9.0`](https://github.com/rexglue/rexglue-sdk/releases/tag/v0.9.0) pointe au contraire sur
[`3eb9b511b4140d2769e27be63eae57d41bfa2afa`](https://github.com/rexglue/rexglue-sdk/commit/3eb9b511b4140d2769e27be63eae57d41bfa2afa),
arbre `a8b23cc4b2ed36ca9ea04a152b14874b43c9ed45`. Une installation « latest » ne
reconstruit donc pas mécaniquement le produit prévu par le HEAD Fable.

## Nature exacte de la migration

La chaîne directe est :

1. `legacy-main` `9890500e...` ;
2. [`85928bf9...`](https://github.com/Fable2Recomp/Fable2Recomp/commit/85928bf9462d9e88e1b27c8ab7c1a4f0027719b9),
   « Rewrite for ReXGlue, Moving old code to Legacy » ;
3. `65ab1651...`, renommage de la configuration et suppression du manifeste
   `vcpkg` ;
4. `f37d8199...`, ajout de quelques frontières ;
5. le HEAD, « Port refii src/thirdparty and config updates ».

Les arbres intermédiaires sont, dans le même ordre,
`bf89971e349e6be48497e55b92a57372e6280d18`,
`1bca2d26fa7e1cbfb9f219e7d43d1da96bb2bf87`, puis
`2b58fcfd3cc952f2c71eb4ec3c6c887ce884fb9c`. Chaque commit a exactement le
précédent comme parent ; il n'y a pas de merge cachant une seconde lignée.

Le commit de migration compte 1095 fichiers touchés, 448 insertions et
11 167 203 suppressions : 1088 suppressions, 2 ajouts et 5 modifications. À
lui seul, il retire le runtime, les backends, les hooks, les outils et tous les
submodules historiques. Dans l'arbre parent se trouvaient 240 fichiers
`src/ppc/ppc_recomp.N.cpp` totalisant 285 274 018 octets, ainsi que 21 blobs
`build/` totalisant 11 938 140 octets. Aucun de ces artefacts n'est le code
exécuté ou construit par le HEAD.

L'ancienne
[`config/config.toml`](https://github.com/Fable2Recomp/Fable2Recomp/blob/9890500e5b0bc4ef17c7d4834e167fdaee483487/config/config.toml#L1-L25)
activait huit optimisations de registres, dont `skip_lr=true`, et déclarait
switch tables, instructions invalides et hooks MIDASM. La
[`Fable2_config.toml`](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/Fable2_config.toml#L1-L13)
HEAD ne porte plus ces contrats. Avec le candidat `v0.2.3`, leur absence
revient aux
[`valeurs par défaut false`](https://github.com/rexglue/rexglue-sdk/blob/88a8034347556cb6da79b92109847220cebfb080/src/codegen/config.cpp#L46-L58),
notamment `skip_lr=false`. Conserver LR
est un bon choix d'observabilité `provisional-rexglue`, pas une validation du
contrôle-flow.

La rupture de provenance est visible jusque dans les adresses spéciales :
`longjmp/setjmp` passent de `0x82CAF450/0x82CBA520` dans `legacy-main`, à
`0x825E22F0/0x83006C90` au premier commit ReXGlue, puis à
`0x82CAFA30/0x83006C90` au HEAD. Sans hash de XEX, région, module, TU et ledger
d'analyse, impossible de distinguer correction de frontière, autre producteur
ou simple dérive. Toutes ces adresses sont `documented-unmatched`.

### Le contrat de migration est lui-même instable

Dans le candidat `v0.2.3`, la commande
[`migrate`](https://github.com/rexglue/rexglue-sdk/blob/88a8034347556cb6da79b92109847220cebfb080/src/rexglue/commands/migrate_command.cpp#L195-L244)
compare puis écrase directement, sans backup, `src/main.cpp` et
`CMakeLists.txt` après confirmation. Les
deux fichiers Fable disent eux-mêmes être « SDK-managed », mais le HEAD y place
précisément ses chemins, son lifecycle, ses backends, son heap et ses sources.
Relancer la migration peut donc supprimer la majorité de l'intégration titre.

Le modèle `v0.9.0` a changé de contrat : le
[`CMakeLists`](https://github.com/rexglue/rexglue-sdk/blob/3eb9b511b4140d2769e27be63eae57d41bfa2afa/resources/templates/init/cmakelists.inja#L1-L24)
inclut désormais `generated/rexglue.cmake`, le
[`bootstrap`](https://github.com/rexglue/rexglue-sdk/blob/3eb9b511b4140d2769e27be63eae57d41bfa2afa/resources/templates/init/rexglue_cmake.inja#L1-L70)
permet `REXSDK_VERSION` avec `find_package(... EXACT)` et configure une cible
depuis un manifeste. Le scanner de migration marque aussi
[`PPC_HOOK/PPC_STUB` comme renommés en `REX_HOOK/REX_STUB`](https://github.com/rexglue/rexglue-sdk/blob/3eb9b511b4140d2769e27be63eae57d41bfa2afa/src/rexglue/commands/migration_scan.cpp#L118-L133).
Le HEAD Fable utilise encore les anciennes macros et n'a ni manifeste, ni
bridge généré, ni version exacte. La compatibilité avec « latest » est
`documented-unmatched`.

Pour AC6, le code titre doit vivre dans des fichiers non générés. Une mise à
jour de SDK doit produire un diff sec, un inventaire des fichiers écrasés, une
sauvegarde et la validation M01 avant adoption ; jamais une régénération
directe sur un worktree contenant les hooks produit.

## Identité retail et table de fonctions

Le README affirme « Fable 2 GOTY TU1 »
([ligne 5](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/README.md#L3-L6)),
mais la configuration ne donne que `assets/game/default.xex`. Elle ne contient
ni SHA-256, ni Title/Media ID, ni région, ni version de module, ni reçu
d'application de TU. Il n'existe donc pas d'identité binaire qualifiée à
laquelle rattacher les 923 lignes de fonctions.

Un parse TOML suivi d'une normalisation numérique donne :

- 923 clés textuelles, mais 922 adresses numériques distinctes ;
- 783 entrées nommées, 140 non nommées ;
- 8 chunks avec parent, 7 valeurs parent distinctes, dont 6 absentes des
  overrides explicites ; ce dernier point n'est pas une erreur en soi, mais
  doit être résolu par un reçu d'analyse ;
- toutes les adresses et tailles sont alignées sur 4 octets ; aucun overlap
  simple entre fonctions autonomes après normalisation.

La collision est concrète :
[`0X82C1BDA8`](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/Fable2_config.toml#L30-L35)
et
[`0x82C1BDA8`](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/Fable2_config.toml#L140-L145)
ont la même taille. Le chargeur candidat accepte les deux syntaxes, les
convertit en `uint32_t`, puis utilise
[`unordered_map::emplace`](https://github.com/rexglue/rexglue-sdk/blob/88a8034347556cb6da79b92109847220cebfb080/src/codegen/config.cpp#L64-L114).
La seconde insertion disparaît silencieusement ; le contrôle de doublons
ultérieur itère la map déjà dédupliquée et ne peut plus voir la collision.
Cette faiblesse du format/loader est `divergent`, la table Fable
reste `documented-unmatched`.

Pour AC6, le linter de pré-codegen doit normaliser toute adresse dans une map
séparée, rejeter les collisions même identiques, vérifier alignement,
overflow, overlap, parents, targets indirectes, switches et plages invalides,
puis sceller le résultat avec le SHA-256 du XEX et la version du générateur.
Les noms `rex_XInputGetState`, `rex_XPhysicalAlloc` ou
`rex_XGGetMicrocodeShaderParts` dans une table sont des labels d'analyse, pas
la preuve que le hook, l'import ou le backend correspondant existe.

L'issue publique
[`#22`](https://github.com/Fable2Recomp/Fable2Recomp/issues/22), créée après le
pin et capturée à `updated_at=2026-06-25T14:39:36Z` sans commentaire
(SHA-256 du corps API
normalisé par `jq -r .body`,
`494e36aeabcf8575b75d0b7b9b57998637bed18626f0d0586f1ed8f226cadbda`),
renforce ce diagnostic sans devenir une validation : avec un SDK de
développement `v0.8.1.4-dev.ge8ce24f`, son auteur rapporte 78 appels non
résolus sur un XEX GOTY/Platinum de base **sans TU fusionné**, alors que le
README annonce TU1. Elle ne donne qu'un SHA-1, signale le mauvais nom de
configuration dans le README, une ambiguïté du champ patch et un segfault du
chemin d'erreur. C'est un rapport tiers `documented-unmatched`, non une preuve
retail de ce HEAD.

## Lifecycle et concurrence

Le premier shell ReXGlue lançait un waiter détaché puis détruisait le runtime
au retour de la fenêtre
([`85928bf9`, lignes 86–119](https://github.com/Fable2Recomp/Fable2Recomp/blob/85928bf9462d9e88e1b27c8ab7c1a4f0027719b9/src/main.cpp#L86-L119)).
Le HEAD corrige ce défaut structurel :

1. construction du runtime et sélection des backends ;
2. `Setup`, initialisation du heap 256 Mio, puis `LoadXexImage` ;
3. création de la fenêtre et des overlays ;
4. lancement différé du module ;
5. à la fermeture, publication de `shutting_down`, `TerminateTitle`, puis
   `join` avant destruction du runtime.

Ces étapes sont visibles dans
[`main.cpp`](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/src/main.cpp#L92-L129),
le lancement
([lignes 171–189](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/src/main.cpp#L171-L189))
et le teardown
([lignes 198–236](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/src/main.cpp#L198-L236)).
Le patron est `provisional-rexglue`.

Il reste incomplet : le `join` n'a aucun timeout, le heap global n'a aucun
`Shutdown`, et aucun test ne couvre une panne après `Setup`, après allocation
du heap, après chargement XEX, ou un second lancement dans le même processus.
Une fermeture pendant le callback différé n'a pas non plus de preuve. AC6 doit
formaliser une machine d'états monotone, rendre le lancement annulable, borner
l'attente du thread invité et vérifier que chaque ressource est relâchée dans
l'ordre inverse, y compris après erreur partielle.

## Heap et mémoire invitée

Le HEAD réserve un mspace `dlmalloc` fixe de 256 Mio par
`SystemHeapAlloc`, traduit son adresse et installe deux hooks
([`heap.cpp`, lignes 28–45](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/src/heap.cpp#L28-L45),
[`116–148`](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/src/heap.cpp#L116-L148)).
La compilation active les mspaces et les locks
([`CMakeLists.txt`, lignes 25–36](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/CMakeLists.txt#L25-L36)).

Cette forme révèle plusieurs contrats à ne pas importer :

- `GuestToHost` recalcule `virtual_membase + guest_addr` au lieu de conserver
  `host_base + (guest_addr - arena_base)`. Cela correspond au heap virtuel
  standard du candidat, mais contourne la traduction officielle, qui peut
  ajouter un `host_address_offset`
  ([`TranslateVirtual`](https://github.com/rexglue/rexglue-sdk/blob/88a8034347556cb6da79b92109847220cebfb080/include/rex/system/xmemory.h#L302-L316)).
  C'est un risque de portabilité `provisional-rexglue`, pas une preuve de bug
  exécuté ici ;
- `arena_base + 256 MiB` n'est pas contrôlé contre l'overflow et le résultat de
  `TranslateVirtual` n'est pas validé ;
- pour un ancien pointeur extérieur à l'arène, `lhHeapRealloc` alloue un bloc
  neuf sans copier `min(old_size,new_size)` et sans libérer l'ancien
  ([lignes 47–90](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/src/heap.cpp#L47-L90)).
  Cette branche perd les données et fuit si elle est atteinte : `divergent` ;
- un `free` extérieur à l'arène réussit silencieusement, et `heap`/`old_size`
  sont ignorés ; aucun census de provenance des pointeurs ne prouve que la
  transition depuis l'ancien allocator est fermée ;
- `PhysicalAllocCached` sert le même mspace **virtuel**, puis retourne une
  adresse virtuelle, sans allocation physique, alias CPU/GPU, watch
  d'invalidation ni hook de libération
  ([lignes 93–112](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/src/heap.cpp#L93-L112)).
  Si la fonction possède bien le contrat physique suggéré par son nom et son
  commentaire, cette substitution est `divergent`; sa portée réelle reste
  `documented-unmatched` faute de callsites et de XEX identifiés ;
- aucun teardown ne détruit le mspace ni ne rend le bloc au system heap.

Sur les trois `PPC_HOOK` du HEAD, seule la cible nommée `rex_lhHeapRealloc`
apparaît dans les overrides
([`0x82885E90`](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/Fable2_config.toml#L220-L226)).
`sub_82B53420` et `sub_82CC7BB8` dépendent de symboles que devrait produire
l'analyseur ; le corpus généré absent ne permet pas même de vérifier leur
liaison. Leur reachability et leur ABI sont `documented-unmatched`.

L'ancien heap o1heap séparait au moins arènes utilisateur et physique, mais il
renvoyait aussi un pointeur factice `0xDEADC0DE` sur certaines erreurs et
contenait ses propres hypothèses. Le remplacer ne qualifie donc ni l'ancien ni
le nouveau modèle.

Le catalogue d'architecture local, vérifié par SHA-256, confirme seulement les
invariants génériques : pointeurs invités 32 bits dans des registres Xenon 64
bits, octets invités big-endian, et ressources Xenos adressées physiquement
avec alignement et invalidation. Il soutient la séparation des domaines ; il
ne constitue aucune preuve Fable ou AC6 retail.

Les quatre entrées relues dans
`../../.tools/knowledge-base/architecture-v1/catalog.json` sont
`xenonrecomp-readme` `b72d34f82f796e4c84f0f622fad8fb720b64607b842de31b70596d35157a68ff`,
`xenosrecomp-readme` `56a7c5074c166377554822b2812830d4f889844c39c10ae89ed5998b29a8f5e1`,
`xenia-shared-memory` `d8be763ff311978594bd30ea0eb5607a57aa6b7358845e1e25f6e93e71dd0eb5`
et `xenia-xenos` `1224073721a11e332dab47e34555a4be6ca9a896ed842915db8e641f391e48c3`.

## VFS, XContent et sauvegardes

Le paramètre CLI nommé `game_directory` est traité comme une racine d'assets,
puis décliné en `game` et `update`. Le runtime est construit avec
`Runtime(game_dir, game_dir, update_dir)`
([`main.cpp`, lignes 49–65 et 88–94](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/src/main.cpp#L49-L94)).

Dans le candidat temporel, les paramètres sont explicitement
`game_data_root`, `user_data_root`, `update_data_root`
([`runtime.h`](https://github.com/rexglue/rexglue-sdk/blob/88a8034347556cb6da79b92109847220cebfb080/include/rex/runtime.h#L84-L114)).
Le premier est
[`monté en lecture seule`](https://github.com/rexglue/rexglue-sdk/blob/88a8034347556cb6da79b92109847220cebfb080/src/system/runtime.cpp#L220-L260)
comme disque titre, tandis que le `ContentManager` reçoit directement le second
([`kernel_state.cpp`, lignes 54–62](https://github.com/rexglue/rexglue-sdk/blob/88a8034347556cb6da79b92109847220cebfb080/src/system/kernel_state.cpp#L54-L62))
et forme des chemins
[`root/title_id/content_type/...`](https://github.com/rexglue/rexglue-sdk/blob/88a8034347556cb6da79b92109847220cebfb080/src/system/xam/content_manager.cpp#L55-L77).
Fable place donc les profils, contenus et saves écrits par ce candidat sous le
répertoire contenant le dump retail.
Cette confusion lecture seule/état utilisateur est `divergent` comme modèle de
packaging, même sans exécution Fable observée.

Le HEAD ne possède aucune implémentation titre de XContent, licence, profil,
save atomique, quota ou confinement. Ces services sont entièrement délégués au
SDK non épinglé : `documented-unmatched`. Les anciens hooks XAM content ont été
supprimés, pas portés.

Le candidat `v0.2.3` ajoute aussi un `NullDevice` pour `Partition0`, `Cache0` et
`Cache1`, avec le but déclaré de faire réussir tous les I/O bruts
([`runtime.cpp`, lignes 264–280](https://github.com/rexglue/rexglue-sdk/blob/88a8034347556cb6da79b92109847220cebfb080/src/system/runtime.cpp#L264-L280)).
Ses lectures et écritures retournent succès sans transférer d'octets ni mettre
à jour les compteurs de sortie
([`null_file.cpp`, lignes 25–48](https://github.com/rexglue/rexglue-sdk/blob/88a8034347556cb6da79b92109847220cebfb080/src/filesystem/devices/null_file.cpp#L25-L48)).
Ce fallback est `divergent` pour un cache/save observé. Comme Fable n'épingle
pas ce SDK et ne publie aucun census de chemins, son exécution ici reste
`documented-unmatched`.

Le hook `GetFileSectorInfo` retourne toujours zéro
([`hooks.cpp`, lignes 16–26](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/src/hooks.cpp#L16-L26)).
Il remplace la position physique du disque par « aucune information » : effet
`divergent` si le seam est atteint, reachability `documented-unmatched`.

AC6 doit utiliser trois racines disjointes : jeu qualifié et immuable, update
qualifié et immuable, utilisateur writable hors assets. Toute écriture save
doit être confinée, atomique, accompagnée d'un test de crash et attribuée au
Title ID/producteur exact ; aucune écriture ne doit modifier la source retail.

## XAM, input et replay

Le HEAD choisit seulement `CreateDefaultInputSystem`
([`main.cpp`, lignes 96–105](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/src/main.cpp#L96-L105)).
Il ne contient ni hook de poll, ni compteur, ni trace, ni enregistrement ou
replay. Les trois labels `XInputGetState` dans la table de fonctions ne prouvent
aucune interception. Tout comportement input est `documented-unmatched`.

Les 22 macros `PPC_STUB` couvrent UI, I/O/objets, partage, réseau, membership,
pays et voix
([`hooks.cpp`, lignes 28–49](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/src/hooks.cpp#L28-L49)).
Dans le candidat `v0.2.3`, `PPC_STUB` journalise mais ne fixe pas `r3`, à la
différence de `PPC_STUB_RETURN`
([`function.h`, lignes 557–583](https://github.com/rexglue/rexglue-sdk/blob/88a8034347556cb6da79b92109847220cebfb080/include/rex/ppc/function.h#L557-L583)).
Tout import sensible au retour peut donc hériter d'une valeur entrante. Cette
sémantique est `divergent` si atteinte ; aucun reachability census n'existe.

Pour M01, le seam d'entrée doit enregistrer au minimum ordre global de poll,
thread, utilisateur, flags, LR, pointeur invité, résultat et octets logiques
du pad. Le replay doit réinjecter au même seam, avec fin de trace explicite et
comparaison des appels supplémentaires/manquants. Un backend hôte par défaut,
une automation UI ou un stub silencieux ne ferme aucune gate de replay.

## XMA, audio, Xenos et GPU

Le shell sélectionne SDL audio, D3D12 si disponible, sinon Vulkan. Il ne
contient aucun hook XMA, ring/context fixture, capture PCM, command processor,
traduction shader, cache de pipeline ou test GPU. Les noms `XG*` et microcode
de la configuration sont des fonctions invitées à recompiler, pas un renderer
hôte. XMA, audio et Xenos sont entièrement `documented-unmatched` derrière le
SDK externe.

Avec le candidat `v0.2.3`, un échec d'initialisation input ou audio journalise
un warning, désactive le sous-système et continue ; un backend graphics nul
n'est pas rejeté par le runtime. Le shell Fable n'ajoute aucune gate. Ce
fail-open est un contre-exemple `divergent` pour un run de qualification : M01
doit échouer avant lancement invité si le lane requis n'est pas réellement
actif.

Le dépôt ne définit aucun lane headless et ne configure pas
`SDL_AUDIODRIVER=dummy`. Il ne remet pas en cause la configuration AC6 déjà
qualifiée : le dummy reste obligatoire pour nos runs Xvfb et doit être testé
séparément d'un lane audio réel. Pour XMA et GPU, seules des fixtures AC6 liées
au XEX PAL — contexte/ring/interruptions/PCM d'une part, paquets/format/endian/
invalidation/fences d'autre part — peuvent faire progresser les gates.

## Build Linux et tests

Le
[`CMakeLists.txt`](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/CMakeLists.txt#L6-L23)
accepte `REXSDK_DIR` ou un package `rexglue` sans contrainte de version. Le
README demande à l'inverse une variable d'environnement `REXSDK`, jamais lue
par ce CMake, et « latest release »
([lignes 14–25](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/README.md#L14-L25)).
Ses commandes clonent `rexglue-sdk` puis font `cd rexglue`, et lancent le
codegen sur `Fable-2_config.toml` alors que le fichier suivi est
`Fable2_config.toml`
([lignes 27–47](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/README.md#L27-L47)).
Le prérequis `vcpkg` n'a plus de manifeste ni de branche CMake correspondante.

Le preset Linux impose `clang-20`/`clang++-20`
([`CMakePresets.json`, lignes 21–33](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/CMakePresets.json#L21-L33)).
Sur l'hôte d'audit, le preset échoue car Clang 20 est absent. Une seconde
configuration ciblée avec Clang 21.1.8 atteint ensuite l'erreur attendue
« ReXGlue SDK not found ». Le SDK n'a pas été récupéré/construit, car le dépôt
n'en déclare pas de pin.

Le CMake prétend rendre `generated/sources.cmake` optionnel, mais les trois
sources titre incluent sans condition les headers `generated/Fable2_*` ; un
build source-only n'est donc pas possible. Il n'existe ni `CTest`, ni tests
titre, ni sanitizer, ni CI, ni reçu de build/release. Les tests présents dans
un SDK candidat ne deviennent pas des tests Fable par transitivité. La
reproductibilité Linux et toutes les assertions multiplateformes du README
sont `documented-unmatched`.

## Licence, origine et octets retail

La racine porte une licence MIT attribuée à RyzenDew
([`LICENSE`](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/LICENSE#L1-L20)).
`src/heap.{h,cpp}` et `src/hooks.cpp` déclarent au contraire BSD 3-Clause,
copyright Tom Clay, puis renvoient vers cette racine MIT
([`heap.cpp`, lignes 1–11](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/src/heap.cpp#L1-L11)).
Le texte BSD requis
n'est pas présent dans l'arbre Fable. `dlmalloc` conserve heureusement son
texte MIT-0 et son attribution Doug Lea
([`dlmalloc.c`](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/thirdparty/dlmalloc/dlmalloc.c#L1-L21)).

Le message « Port refii src/thirdparty and config updates » ne fournit ni URL,
ni commit, ni arbre, ni licence de la source `refii`. Le candidat ReXGlue est
lui-même BSD 3-Clause, mais cette proximité ne constitue pas un reçu d'origine
pour les fichiers Fable. La redistribution exige donc une réconciliation
explicite des notices et une provenance fichier par fichier. Tout cela reste
`documented-unmatched`, sans conclusion juridique automatique.

Au HEAD, les chemins `assets/*` et `generated/*` sont ignorés
([`.gitignore`, lignes 402–406](https://github.com/Fable2Recomp/Fable2Recomp/blob/1e25911172f8e30458099eda96a1ad7b8992ed60/.gitignore#L402-L406))
et aucun XEX, ISO, asset ou fichier généré n'est suivi. Une recherche des noms
sur tout l'historique ne trouve pas de chemin XEX/ISO, mais trouve de nombreux
binaires tiers et artefacts de build. L'historique contient surtout les 285 Mo
de C++ PPC généré précités, potentiellement dérivés du retail. Le contrôle par
chemin ne prouve pas le contenu de chaque blob historique. Aucune sémantique
n'a été tirée de ces sources générées ; le diff statistique a toutefois pu
hydrater des blobs publics dans le clone partiel. Un inventaire qui veut éviter
ce coût doit rester sur `ls-tree` et ne pas demander de statistiques de lignes.

L'audit a utilisé un clone partiel sans checkout historique, n'a téléchargé,
ouvert ou exécuté aucun XEX, asset ou conteneur retail, et n'a lancé ni codegen
ni runtime. Une revue externe doit de même recevoir seulement des plages bornées avec
offset/longueur/but/SHA-256 source, jamais un conteneur retail complet.

## Contrats à retenir pour AC6 Mission 01

| Domaine | Contrat/gate AC6 | Classe de l'apport Fable |
|---|---|---|
| dépendances | pin commit + tree du SDK et de chaque submodule ; refuser `latest` et les packages sans version exacte | `provisional-rexglue` |
| migration | code titre hors fichiers SDK-managed ; dry-run, sauvegarde, diff et replay M01 avant adoption | `provisional-rexglue` |
| producteur | SHA-256 XEX, Title/Media ID, région, module/TU, projet Ghidra et target ID dans chaque reçu | absence Fable `documented-unmatched` |
| codegen | normalisation numérique avant insertion ; doublons, overflow, overlap, parent, switch et cible indirecte rejetés | collision Fable `divergent` |
| lifecycle | états monotones, lancement annulable, `TerminateTitle`, join borné, teardown inverse et restart testé | ordre Fable `provisional-rexglue` |
| heap | provenance de chaque pointeur ; realloc étranger copie ou échoue ; aucun free silencieux ; teardown explicite | branches Fable `divergent` |
| mémoire physique | allocation/alias/invalidation/fence Xenos distincts du heap virtuel | substitution Fable `divergent` |
| VFS/save | game/update immuables, user séparé, chemins confinés, writes atomiques et crash tests | racines Fable `divergent` |
| XAM/imports | ABI, résultat et effets secondaires explicites ; census des appels ; aucun stub laissant `r3` indéfini | stubs Fable `divergent` |
| input/replay | capture et réinjection poll-exactes au seam invité avec LR/thread/user/flags/résultat/octet | Fable `documented-unmatched` |
| XMA/GPU | fixtures et traces PAL M01 ; backend effectivement actif ; aucun fallback silencieux | Fable `documented-unmatched` |
| Linux/tests | toolchain et SDK hermétiques, configure/build/test headless, pannes partielles, restart et absence de retail | Fable `documented-unmatched` |
| licence/package | SBOM, NOTICE, origine/hash par fichier, audit récursif du paquet et exclusion retail | Fable `documented-unmatched` |

Ce dépôt n'apporte donc aucun composant à copier dans AC6. Il apporte surtout
un jeu précis de tests de non-régression à ajouter autour de notre migration :
collision d'adresses sensible à la casse, realloc d'un pointeur pré-hook,
allocation dite physique, fermeture pendant le lancement différé, teardown
après chaque étape, séparation save/assets, import à retour non défini et
build avec mauvais SDK.

## Validation reproductible et limites

Les opérations suivantes ont été effectuées sur clones temporaires hors du
worktree :

- `git ls-remote --heads --tags`, puis clone partiel `--filter=blob:none` et
  checkout détaché du HEAD ;
- `git cat-file -p`, `git ls-tree -r -l`, comptage des gitlinks et comparaison
  directe `9890500e..1e259111` ;
- inspection ciblée des quatre commits de migration et des sources ReXGlue
  `v0.2.3`/`v0.9.0` épinglées ;
- parse TOML, normalisation `int(key, 0)`, contrôles d'alignement, collisions,
  parents et overlaps ;
- deux configurations CMake Linux sans SDK ni retail ;
- validation de chaque cible citée : objets Git locaux, bornes de lignes, puis
  HTTP 200 via `raw.githubusercontent.com` ou l'API GitHub ;
- vérification SHA-256 des entrées XenonRecomp, XenosRecomp et Xenia mémoire du
  catalogue d'architecture local.

Limites résiduelles : aucun binaire Fable n'est publiquement qualifié par ce
HEAD, le SDK réellement utilisé demeure inconnu, le corpus généré n'est pas
présent, et l'issue #22 concerne une révision ultérieure. Il est donc impossible
de valider reachability, ABI, rendu, XMA, input, save ou lifecycle en exécution.
Ces absences interdisent toute promotion au-delà des classes consignées ici.

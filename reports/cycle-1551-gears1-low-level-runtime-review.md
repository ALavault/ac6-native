# Cycle 1551 — audit bas niveau du runtime Gears 1

Date : 12 août 2026.

## Résultat

Le dépôt Gears 1 est une bonne source de **formes d'instrumentation et de
tests différentiels**, mais pas un backend à transplanter. Son XMA descend de
la même lignée Xenia/ReXGlue que notre oracle PAL, son renderer est fortement
adapté à Gears/UE3 et son command processor contient quatre erreurs certaines
qui interdisent de le prendre comme référence d'exécution : longueur TYPE1,
effets d'un paquet ring incomplet, endian modes 1/3 et comparateur WAIT 0.

Pour AC6, les apports prioritaires sont donc : un replay XMA à deux buffers et
ring plein, un décodeur PM4 pur qui valide avant tout effet, des fixtures de
texture/mips/alias EDRAM et des tests XAM au seam invité. Tout doit être
réécrit ; aucune source Gears ne peut être copiée faute de licence racine.

## Source et provenance

- dépôt : `https://github.com/SomeoneIsWorking/gears1` ;
- HEAD audité : `5e19554128026200fa201d2dbbfa737a2ae8ec1f`, daté du
  12 août 2026, checkout propre ;
- sous-modules déclarés dans `.gitmodules:1-10` : XenonRecomp
  `e481deca4b0e6fe1e9ebf8058e1837f9e6848eb5`, FFmpeg-XMAFrames
  `d980192e175e6ff95bcd287af77e16fcb6597974`, Xenia/Canary
  `a54abbc530f2530f8607fc2b9eabaccf27f49505` ;
- le pin XenonRecomp est MIT ; le fork FFmpeg est LGPL-2.1+ par défaut et est
  lié statiquement par `cmake/ffmpeg_xma.cmake:42-75` ;
- aucun `LICENSE`, `COPYING` ou `NOTICE` à la racine, et GitHub ne détecte
  aucune licence pour le projet ; le code propre au runtime est donc non
  transplantable ;
- le gitlink Xenia exact n'était plus récupérable depuis son remote public au
  jour de l'audit (`upload-pack: not our ref`, API GitHub 422). La traduction
  SPIR-V exacte n'est plus reproductible depuis les références publiées ;
- `CMakeLists.txt:18-22` suit le `main` mouvant de Lucent ; aucune CI publique
  n'est présente. `tests/CMakeLists.txt` déclare 31 CTest, et non les 20 de la
  matrice initiale, mais aucun test automatique XMA, PM4, tiling, mip ou EDRAM.

Le catalogue local `.tools/knowledge-base/architecture-v1/catalog.json` a été
consulté comme preuve générique seulement. Il dit explicitement ne pas être une
preuve retail. La politique AC6 reste celle de
`analysis/rexglue-semantic-trust-v1.json:37-51` : une sémantique ReXGlue atteinte
peut servir au bring-up et aux tests sous `provisional-rexglue`, mais ne ferme
aucune lane et ne devient `retail-qualified` qu'avec identité PAL, observation
retail bornée et test natif.

## XMA et timing audio

### Invariants recoupés avec le SDK PAL

Les éléments suivants concordent avec le SDK ReXGlue épinglé dans l'oracle PAL
et deviennent donc des candidats `provisional-rexglue` **seulement après un
census d'atteinte AC6** :

- fenêtre MMIO `0x7FEA0000`, registres array/next/kick/lock/clear
  `0x600/0x607/0x650/0x690/0x6A0`, 320 contextes de 64 octets et paquets de
  2 Kio : `runtime/xma.cpp:29-45` ;
- registres little-endian mais contexte en seize mots big-endian :
  `runtime/xma.cpp:53-62`, `runtime/xma_context.cpp:172-188` ;
- une écriture MMIO reste une écriture mémoire **et** devient un événement :
  `runtime/ppc_mmio.h:5-22`, `runtime/ppc_mmio.cpp:13-28` ;
- kick visible synchroniquement au retour du store : `runtime/xma.cpp:289-320`.
  Le SDK PAL fait aussi `Enable`, réveille le worker puis attend sa fin dans
  `thirdparty/rexglue-sdk/src/native/audio/xma/decoder.cpp:533-549` ;
- offsets d'entrée en bits, double-buffer, ring PCM par blocs de 256 octets et
  fusion dans un snapshot frais : `runtime/xma_context.h:27-139`,
  `runtime/xma_context.cpp:857-880`.

### Divergences et défauts

- Le store de lock ne change aucun état (`runtime/xma.cpp:333-340`). Le SDK PAL
  désactive le contexte puis attend la sortie du worker
  (`decoder.cpp:550-559`) : **`divergent`**.
- `interruptWhenDone` est explicitement absent
  (`runtime/xma_context.cpp:906-909`) : `retail-needed`, puis `divergent` s'il
  est atteint.
- `XmaContextData` utilise des bitfields C++ (`runtime/xma_context.h:31-82`).
  L'assertion de taille ne prouve pas leur ordre ; AC6 doit lire seize `be32`
  par masques et décalages explicites.
- La détection d'un header XMA1 coupé entre paquets par le bit de continuation
  (`runtime/xma_context.cpp:280-377`) est intéressante, mais seulement mesurée
  sur Gears et sans fixture publique : `retail-needed` pour AC6.
- Boucles, vrai double-buffer et plusieurs taux restent déclarés non exercés
  dans `docs/re-frontier.md:182-188`.
- Sans codec, Gears fige volontairement l'offset
  (`runtime/xma_context.cpp:675-683`), alors que le SDK PAL consomme le frame :
  divergence connue.
- La conversion float vers PCM (`runtime/xma_context.cpp:703-714`) mappe `-1`
  sur `-32767` et ne traite pas explicitement NaN/Inf.
- La fusion réécrit `outputBufferReadOffset` alors que le header le décrit comme
  appartenant au titre (`runtime/xma_context.h:75-79`,
  `runtime/xma_context.cpp:857-879`). Le SDK PAL partage ce risque ; une fixture
  de mutation concurrente est nécessaire.

### Codec et replay

Le fork FFmpeg épinglé expose `AV_CODEC_ID_XMAFRAMES`, un frame reconstruit en
entrée, et son commit fixe explicitement `skip_frame=0` dans
`libavcodec/wmaprodec.c:2136-2244`. Les commentaires Gears disant qu'un premier
`EAGAIN` est normal sont donc périmés (`runtime/xma_decode.h:41-43`,
`runtime/xma_decode.cpp:125-127`). Le SDK PAL possède déjà le même seam FFmpeg
et appelle correctement `av_packet_unref` avant réutilisation ; Gears ne le fait
pas (`runtime/xma_decode.cpp:106-148`).

La corrélation documentée à 1,0 n'est pas une validation codec indépendante :
golden file-level et candidat frame-level partagent FFmpeg/WMA Pro. Elle valide
surtout packet walk et ring contre une autre packetisation. En outre :

- `tools/xma_replay.cpp:135-159` ne capture qu'un buffer ; un second buffer
  distinct est désactivé ;
- son drain repose sur `read != write` (`:188-205`), mais la production encode
  un ring plein par `read==write` et `outputBufferValid=0`
  (`runtime/xma_context.cpp:979-982`) : le replay ne peut pas valider ce cas ;
- `tools/xma_compare.py:54-108` choisit un lag global sur une seule fenêtre et
  ne détecte pas une dérive locale ; corrélation `0.99..0.999` retourne tout de
  même succès (`:110-118`) ;
- `xma_replay` n'est pas un CTest et exige des dumps privés
  (`tests/CMakeLists.txt:26-45`).

La politique pure de retard/rebase du pump est une bonne forme de test
(`runtime/audio_pace.h:48-70`), mais le pump hôte à 187,5 Hz n'est pas une
preuve de timing APU. `XAudioUnregisterRenderDriverClient` ne retire ni callback
ni contexte et n'arrête pas le thread (`runtime/xaudio_null.cpp:444-449`) ; les
deux globaux sont non atomiques. Le `RetailMediaDecoder` AC6 qui décode le vrai
`bgmpack.bin` reste une frontière file-level PAL déjà qualifiée et ne doit pas
être remplacé par ce modèle MMIO (`reports/checkpoint-2-media-xma-contract.md`).

## Input et XAM

La timeline Gears n'est pas un replay poll-exact :

- steps millisecondes et `VdSwap` sont mélangés et triés sur la même valeur
  brute (`runtime/input.cpp:29-47,110-157`) ; un step non dû d'une unité peut
  bloquer un step dû de l'autre ;
- `from_chars` ne vérifie pas la fin du token (`:135-140`) et un bouton inconnu
  produit silencieusement un masque zéro (`:81-99,142-153`) ;
- toutes les étapes dues sont consommées, seule la dernière est publiée
  (`:287-327`) : une pression entre deux polls disparaît ;
- les scripts prétendent à tort qu'un run lent ne désynchronise pas
  (`tools/capture_gameplay_frame.sh:16-18`,
  `tools/run_to_checkpoint.sh:17-19`).

Le replay AC6 de `reports/cycle-1539-synchronized-oracle-input-replay.md:7-24`
est déjà supérieur : `poll_index` global, LR/user/flags/null/status/état, et
tick/present seulement diagnostiques. Gears ne justifie aucune migration ; il
suggère seulement des tests négatifs contre unités mixtes, tokens suffixés,
boutons inconnus et edges non observés.

Les bonnes formes XAM sont les tests au seam invité avec arguments voisins
leurres, notamment le neuvième argument de `XamContentCreateEx` à `r1+84`
(`tests/test_guest_stack_argument.cpp:46-108`). Cette adresse est prouvée pour
le callsite Gears (`runtime/xam_user.cpp:533-570`), pas pour AC6 PAL.

Limites importantes :

- `tests/test_xam_user.cpp:145-149` ne couvre que trois scénarios
  `XamUserGetName`, aucun `XamInput*` ;
- une assertion XCONTENT est neutralisée par `|| true`
  (`tests/test_xam_content.cpp:122-124`) ;
- pas de test rumble, changement de device, notification, enum async ou
  annulation overlapped ;
- `XamContentGetCreator` et `XamContentDelete` ignorent leur overlapped
  (`runtime/xam_user.cpp:657-704`) ;
- `CreateAlways`/`TruncateExisting` décident `Create`, mais l'effet ne supprime
  pas l'ancien contenu (`runtime/xam_content.cpp:27-44`,
  `runtime/xam_user.cpp:620-638`) ;
- la capacité 512 Mio est un contournement spécifique Gears
  (`runtime/xam_user.cpp:448-465`).

## PM4, Xenos et présentation

### Erreurs certaines du command processor

1. `ExecutePacket` calcule un count depuis les bits 16..29 pour tous les types
   (`runtime/vd_null_gpu.cpp:2681-2692`) et le retourne aussi pour TYPE1
   (`:2755-2760`). Or TYPE1 porte toujours exactement deux payloads ; ces bits
   contiennent l'index du second registre. Le catalogue local
   `xbox360/xenia-xenos.h:1645-1650` ferme cette preuve : désynchronisation du
   ring, **`divergent`**.
2. Le ring calcule `available`, mais passe la taille totale du ring à
   `ExecutePacket` (`vd_null_gpu.cpp:2880-2895`). Des payloads stale peuvent
   donc produire des effets avant que l'overshoot ne soit détecté à
   `:2897-2918`. Le SDK PAL prévalide TYPE0/TYPE3 avant toute écriture
   (`thirdparty/rexglue-sdk/src/graphics/command_processor.cpp:805-863`).
3. `StoreEndian` rabat les modes 1/3 sur 2 et `LoadEndian` tout mode non nul sur
   2 (`vd_null_gpu.cpp:427-453`), tandis qu'un helper correct existe déjà à
   `:2247-2264`.
4. WAIT traite par défaut comparateurs 0 et 7 comme vrais
   (`vd_null_gpu.cpp:2637-2647`). Le SDK PAL établit 0=`Never`, 7=`Always`
   (`command_processor.cpp:1142-1166`). L'intervalle est ignoré et un wait
   bloqué n'a pas de sortie.

Les IB sont seulement bornés à huit niveaux, sans détection de cycle
(`vd_null_gpu.cpp:2308-2328`). L'opcode swap privé `0x7F`, son bloc de 64 dwords,
ZPD « tout visible » et EXT surface complète sont des ABI/heuristiques Gears,
pas des faits Xenos (`vd_null_gpu.cpp:252-260,2484-2583,3238-3320`) :
`divergent` pour AC6.

### Renderer Vulkan

- La réutilisation intéressante est la séparation du traducteur microcode Xenia
  vers SPIR-V (`xenia_gpu/CMakeLists.txt:71-123`) du CP/resources maison. Le pin
  source disparu empêche néanmoins sa reproduction exacte.
- Detiling 2D/3D/cube et XOR endian sont présents dans
  `runtime/gpu_draw_xlate.cpp:1523-1695`, mais seul mip 0 est demandé
  (`:1589-1594`) ; image et vue Vulkan n'ont qu'un niveau
  (`gpu_draw_textures.cpp:218-268`) et les textures mip-only avec `basePage==0`
  sont ignorées (`gpu_draw_xlate.cpp:1561-1564`).
- L'anisotropie est mise dans la clé mais jamais activée ; border noir fixe
  (`gpu_draw_textures.cpp:336-359`). La constante des vues signed reste zéro
  (`:495-515`).
- L'EDRAM devient une image hôte pleine, mono-échantillon, par base
  (`gpu_draw_targets.cpp:93-188`) et non une mémoire de 10 Mio bit-adressable.
  Les recouvrements partiels de bases ne sont pas représentés. Les resolves
  sont élargis vers `R16G16B16A16_SFLOAT`/`R32_SFLOAT`
  (`gpu_draw_targets.cpp:318-423`) et un chemin SNORM est explicitement lossy
  (`:170-187`).
- Le render thread supprime une frame lorsqu'il est occupé
  (`runtime/render_thread.cpp:117-140`). MAILBOX, sinon FIFO, suit le scheduling
  hôte (`gpu_present.cpp:526-543`). Le handshake booléen `serviced` avec timeout
  500 ms (`gpu_present.cpp:1416-1435`) peut confondre un acquittement tardif et
  la requête suivante.

## VFS et saves

- Cdrom, les deux partitions HDD, SystemRoot et `game:/d:` pointent tous vers le
  dossier jeu (`runtime/guest_filesystem.cpp:19-40`) : convenance de titre,
  jamais topologie Xbox générique.
- `Resolve` concatène le suffixe invité sans rejet de `..` ni vérification de
  containment/symlink (`guest_filesystem.cpp:140-173`) : évasion possible hors
  racine. Le walk case-insensitive appelle `tolower(char)` sans cast unsigned
  (`:175-210`).
- Le `shared_ptr` d'`OpenFile` protège contre close/use-after-free
  (`runtime/kernel_file.cpp:41-65`), mais aucune serrure ne protège
  `fseek/fread/fwrite` sur un même handle.
- Read/Write ignorent les erreurs de seek et les bornes guest ; Write retourne
  succès même en écriture courte et `fflush` ne fournit ni transaction ni
  `fsync` (`kernel_file.cpp:309-425`).
- Un fichier zéro octet ne constitue jamais un contenu
  (`runtime/xam_content.cpp:47-58`). Aucun test containment, symlink, write
  court, concurrence ou atomicité n'est présent.

## Décision de confiance AC6

| État | Frontières |
|---|---|
| `provisional-rexglue` | Après preuve d'atteinte PAL : map MMIO XMA, 320×64, mots contexte BE/registres LE, kick synchrone, clear, géométrie packet/frame, ring et merge déjà présents dans le SDK PAL. PM4 seulement selon le SDK/catalogue épinglé, jamais selon l'exécuteur Gears. |
| `retail-qualified` | Aucun nouvel élément apporté par Gears. Le décodage file-level de `bgmpack.bin` reste une qualification AC6 séparée. |
| `divergent` | Lock ignoré, freeze sans codec, interrupt absent s'il est atteint ; timeline wall/frame ; quatre défauts PM4 ; swap/ZPD/EXT privés ; mip0-only, signed absent, EDRAM par image-base ; alias VFS et capacité 512 Mio. |

À qualifier sur le retail PAL : accès MMIO XMA, contextes/rates/loop/double
buffer/interrupt ; signatures et overlapped XAM ; opcodes PM4/endian/IB ; formats
fetch, mips, cubemaps et alias EDRAM ; chemins VFS effectivement atteints.

## Tranches manuscrites recommandées

1. **P0, contrat XMA pur en shadow** : seize mots BE explicites, registre MMIO
   événementiel et automates kick/lock/clear/release. Injecter un faux décodeur
   PCM pour tester le protocole indépendamment de FFmpeg.
2. **P0, replay XMA à deux buffers** : occupation du ring explicite ; fixtures
   header scindé, `0xFF`, frontière buffer, loop, quatre rates, mono/stéréo,
   erreur codec et ring plein. Gate par longueur/SHA PCM, RMS/pic et lag par
   fenêtres ; `MARGINAL` doit échouer.
3. **P0, PM4 decode-then-execute** : calcul de longueur pur, aucune mutation
   avant paquet complet, TYPE1 à deux mots, wrap/troncature, quatre endian,
   WAIT 0/7, IB cycle/depth. Lancer d'abord en census à côté du ReXGlue
   provisoire.
4. **P1, input** : conserver `poll_index` AC6 ; ajouter uniquement les fixtures
   négatives révélées par Gears.
5. **P1, texture/EDRAM** : fetch constants synthétiques couvrant endian,
   2D/3D/cube, mip-only/packed, signed et upload in-place ; modèle d'intervalles
   EDRAM avant toute image par base.
6. **P2, XAM/VFS** : tests ABI avec registres leurres, overlapped explicite,
   normalisation fail-closed, containment/symlink, writes courts et publication
   atomique côté hôte.

Les fixtures retail éventuelles doivent rester des tranches bornées décrites
par offset, longueur, usage et SHA-256 de source ; aucun container retail ne
doit entrer dans le rapport ou le dépôt.

## Validation et risques résiduels

Audit en lecture seule du dépôt externe ; aucun build lourd, XEX ou octet retail
n'a été ouvert. Le checkout Gears était propre après lecture. Les assertions
sensibles ci-dessus ont été recoupées dans le code, dans le SDK PAL épinglé et,
pour TYPE1, dans le catalogue d'architecture local. Risques restants : pin Xenia
irrécupérable, licence racine absente, résultats XMA privés non reproductibles,
absence de CI et dépôt extrêmement mouvant.

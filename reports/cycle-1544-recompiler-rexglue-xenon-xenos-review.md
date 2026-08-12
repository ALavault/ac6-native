# Cycle 1544 — rexdex, ReXGlue, XenonRecomp et XenosRecomp

## Décision

ReXGlue devient la référence d'exécution provisoire de Mission 01. Cela permet
d'avancer sur le cône réellement atteint sans qualifier préalablement toute la
Xbox 360. La présomption est volontairement asymétrique : un comportement
ReXGlue implémenté et sans contradiction connue peut être
`provisional-rexglue`; un stub, un no-op matériel, une approximation connue ou
une contradiction est immédiatement `divergent`.

Cette confiance est attachée à une révision précise. Le PAL actuel exécute un
arbre ReXGlue 0.7.1 ; les apports de ReXGlue 0.9, du fork Skate ou du build US
ne lui sont pas attribués sans diff sémantique. La qualification retail est
différée jusqu'à ce que le replay ait fourni le census exact de M01, mais reste
obligatoire avant fermeture de lane et publication.

Les autres projets gardent des rôles distincts :

- XenonRecomp/XenonAnalyse : preuve déterministe de décodage, contrôle de flux
  et ABI, sans runtime ;
- XenosRecomp : bootstrap et deuxième lecture des shaders, sans command
  processor ;
- rexdex/recompiler : référence historique d'architecture bas niveau, jamais
  oracle AC6 ;
- UnleashedRecomp : preuve qu'une frontière D3D haute peut remplacer PM4 pour
  un titre, pas runtime Xbox générique ;
- Skate3Recomp : référence de migration hybride et d'instrumentation.

## Sources épinglées

| Projet | Commit / arbre | Licence | Usage AC6 |
|---|---|---|---|
| rexdex/recompiler | `7cd1d5a33d6c02a13f972c6564550ea816fc8b5b` | MIT racine, provenance par fichier à auditer | Architecture historique seulement |
| RexGlue upstream 0.9.0 | `cb58065c793429aa92895d778af58d12e9d26d8f`, arbre `a8b23cc4…` | BSD-3-Clause | Référence sémantique provisoire moderne |
| AC6 ReXGlue PAL | AC6 `dcd41b7457fcac8242f8ef40de83d1719390d5af`, arbre SDK `741541d6…`, 0.7.1 | BSD-3-Clause | Oracle PAL exécuté actuel |
| AC6 ReXGlue US moderne | AC6 `ab90b54713e5889f33eee1cc8681dae89fe83d1e`, arbre SDK `73589e54…`, 0.8.0 | BSD-3-Clause | Référence US seulement, identité XEX manquante |
| ReXGlue Skate | `7eb0faf7787f5e01333c228b8e3f03c32f7295ea` | BSD-3-Clause | Fork titre, architecture seulement |
| XenonRecomp local | `ddd128bcca99fe8bfbb99bea583c972351fa6ace` | MIT | Générateur déterministe |
| XenonRecomp Unleashed | `c5bfd90d87f2ed0db8cff5c19ea3aff0e161e527` | MIT | Pin du projet Unleashed |
| XenosRecomp | `990d03b28a27b50277ee5d8d942e1c5f873869d1` | MIT | Traducteur shader offline |
| UnleashedRecomp | `cf829a9eca8fb680fba4b0409ddeb6ca92f22e3c` | GPLv3 | Inspiration sans copie implicite |
| Skate3Recomp | `f6e0ae87fdfecbadb5c1e36c55d66a744187a3cd` | licence racine absente | Lecture conceptuelle seulement |

Les identités PAL restent `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`,
projet canonique `ghidra-projects/ace-combat-6`. Aucun pin tiers ne remplace
cette identité.

Le dépôt AC6 moderne annonce `rapidsamphire/rexglue-sdk` dans `.gitmodules`,
mais `thirdparty/rexglue-sdk` est stocké comme un répertoire Git normal, pas un
gitlink. Le fork déclaré et l'arbre réellement compilé ne coïncident pas ;
l'arbre `73589e54…` est donc l'unique identité utile. Depuis l'upgrade 0.8, cet
arbre a encore divergé sur 83 fichiers. Le même principe vaut au PAL : seul
`741541d6…`, pas un nom de branche, identifie le SDK exécuté.

## Matrice des architectures

| Composant | CPU AOT | Runtime/HLE | PM4/EDRAM | Shaders | Linux | Position |
|---|---:|---:|---:|---:|---:|---|
| rexdex | Oui, MSVC | Oui, très incomplet | Oui, DX11 incomplet | Runtime HLSL | Non | Historique |
| XenonRecomp | Oui | Non | Non | Non | Oui | Oracle déterministe |
| XenosRecomp | Non | Non | Non | Offline HLSL/DXIL/SPIR-V | Oui | Oracle shader |
| ReXGlue | Oui | Oui | Oui, Xenia-derived | Runtime DXBC/SPIR-V | Oui selon titre | Oracle provisoire |
| Unleashed | XenonRecomp | Micro-runtime Sonic | Contourné par hooks D3D | XenosRecomp offline | Oui | Patron D3D haut |
| Skate | ReXGlue | ReXGlue | ReXGlue conservé | Runtime + shaders natifs Skate | Oui | Patron hybride |
| AC6 natif | Aucun généré | C++ manuscrit | `DrawPacket` Vulkan | C++/SPIR-V produit | Oui | Produit final |

## rexdex/recompiler

### Valeur réelle

Le projet historique charge/déchiffre/décompresse un XEX, produit un projet
binaire PDI/REP, génère des blocs C++ MSVC dans une DLL renommée `code.bin`,
mappe la mémoire invitée aux adresses basses, résout des imports Windows et
exécute un backend Xenos DX11.

Sa partie la plus intéressante est sa séparation bas niveau :

```text
MMIO -> CP_RB_WPTR -> ring big-endian -> packets PM4
     -> état Xenos -> shaders/surfaces/EDRAM -> backend
```

Elle fournit des idées de fixtures pour wrap du ring, packets type 0/3,
indirect buffers, waits, registres, quatre modes endian et tiling. Ses wrappers
BE typés améliorent aussi la lisibilité des structures invitées.

### Pourquoi il ne remplace rien

Le README dit qu'aucun jeu réel n'a été essayé. L'analyse est linéaire et
heuristique, sans `.pdata`; elle transforme toute cible `bl` en fonction,
scanne les mots de données comme pointeurs et n'a pas de solution robuste pour
les indirects/switches. Le binder ABI mélange mal les compteurs GPR/FPR, part
de `fr0`, n'implémente pas les arguments stack/vector et tronque un retour
64 bits.

Le modèle mémoire cast directement adresse invitée vers pointeur hôte. Les
atomiques utilisent une réservation globale imparfaite, les barrières sont
vides, le FP/VMX décompose des opérations fused et emploie des réciproques
exactes. XMA est essentiellement absent. Le backend force DX11, un mip, une
seule slice cubemap et laisse de nombreux formats/resolves/cohérences TODO.

Ces comportements sont `divergent`. rexdex n'hérite jamais du statut
`provisional-rexglue`. Sa licence MIT racine ne suffit pas à autoriser une
copie aveugle : plusieurs fichiers GPU citent Code Aurora, Freedreno, Crunch ou
Inferno Engine. On reprend les interfaces et tests, pas les sources.

## XenonRecomp et XenonAnalyse

XenonRecomp convertit littéralement chaque instruction PPC en C++ autour d'un
`PPCContext` et d'une base mémoire. Les GPR restent 64 bits, les pointeurs
invités 32 bits, les accès mémoire inversent l'endian, et les vecteurs sont
stockés avec ordre hôte inversé. Il distingue denorm FPU et flush VMX et offre
des fonctions faibles/mid-hooks faciles à remplacer.

Il ne fournit volontairement ni runtime, ni MMIO, ni exceptions. Les indirects
utilisent une table construite après l'image. XenonAnalyse trouve des candidats
de frontières via `.pdata`/`bl` et quatre motifs de switch dans une fenêtre de
32 instructions ; ces candidats ne supplantent jamais Ghidra.

Dans ce dépôt, la génération PAL qualifiée a déjà servi à :

- reproduire les switch tables et compiler syntaxiquement 81 unités ;
- ajouter un `lhbrx` déterministe et testé ;
- transformer `dcbst` en hook `PPC_DCBST(EA)` surchargeable ;
- cross-matcher du contrôle de flux/ABI sans copier le généré.

Le défaut du générateur est moins fail-closed que ReXGlue 0.9 : certaines
instructions manquantes laissent un commentaire/warning avant poursuite. Les
sorties doivent donc toujours être auditées pour traps/diagnostics. Son MFTB
basé sur `rdtsc`, ses atomiques et ses barrières sont divergents comme runtime.

Le bon rôle ne change pas : générateur et analyseur déterministes, jamais code
produit ni vérité sémantique par leurs noms.

## XenosRecomp

XenosRecomp lit un conteneur shader reverse-engineeré pour Sonic, traduit le
microcode en HLSL, compile DXIL/SPIR-V et construit un cache offline indexé par
XXH3-64. Il ne traite ni PM4, ni EDRAM, ni resolves, ni untile/endian des
textures.

Limites confirmées : contrôle complexe peu testé, indexation dynamique et
constantes entières absentes, partie des texture fetch silencieusement ignorée,
mini-fetch, memexport, point size et plusieurs modes sampler/LOD absents ;
formats et vertex locations portent des hypothèses Unleashed.

Il reste très utile à AC6 comme deuxième traduction : notre scanner PAL a déjà
reproduit son hash brut et joint deux shaders exacts du corpus. La suite est de
recenser le seul sous-ensemble M01, puis de couvrir chaque opcode, fetch,
format, constante et spécialisation par golden tests. Aucun shader généré
n'entre dans le produit.

## ReXGlue : la référence provisoire

### Pourquoi elle accélère réellement

ReXGlue rassemble codegen, mémoire invitée, dispatcher, threads, kernel,
XAM, VFS, input, XMA/FFmpeg et command processor Xenia-derived D3D12/Vulkan.
Il fournit donc immédiatement les observations qu'il faudrait des mois à
recréer : imports atteints, cadence de polls, états Xenos, shaders, textures,
resolves et frames.

Le codegen 0.9 dispatche environ 508 identifiants PPC vers 391 builders ; une
instruction absente émet `REX_UNIMPLEMENTED`, donc un arrêt visible. Il
supporte `.pdata`, configs de fonctions/chunks, jump tables, invalid
instructions, appels indirects connus, hooks faibles et mid-asm.

La présomption raisonnable demandée s'applique ainsi : toute sémantique
atteinte, réellement implémentée dans la révision exécutée et sans divergence
connue est utilisable pour le bring-up natif. Il n'est plus nécessaire de
micro-exécuter chaque feuille avant de l'implémenter.

Le registre HLE large ne prouve rien à lui seul. Le census statique donne 2 620
fonctions enregistrées dans les SDK examinés, environ 460 implémentations et
2 149 à 2 407 stubs explicites selon le fork. Le replay M01 doit donc produire
la liste des ordinals réellement atteints et arrêter au premier stub.

La bonne frontière input se trouve dans `XamInputGetState_entry`, autour de
tous ses retours, et non dans `InputSystem::GetState` : ce dernier a déjà perdu
les retours XAM courts et fusionné les pilotes par OR/max/axe de plus grande
magnitude. Le contexte courant permet de capturer LR et thread. AC6 ne définit
pas `skip_lr`, contrairement à Unleashed ; la garde LR est donc disponible dans
l'oracle PAL tant que le manifeste l'impose.

Le loader sait afficher title/media/version pendant le codegen, mais l'image
générée et le lancement ne scellent pas ces champs. Le gate AC6 moderne ne
compare que le Title ID, commun aux régions : il pourrait accepter un PAL avec
une carte d'adresses US. Le sidecar oracle doit donc ajouter SHA XEX, hash du
module chargé, Media ID, version/base et hash des bytes du marqueur.

### Pourquoi elle reste provisoire

La suite de tests n'est ni activée par défaut ni lancée par la CI de release :
`REXGLUE_BUILD_TESTS=OFF`, 166 fixtures assembleur seulement si activées, et
aucun `ctest` dans le workflow de build. Le template ne sait pas comparer
FPSCR ni VSCR.SAT ; les 25 fixtures VMX128 n'exercent pas `v32..v127`.

Les divergences confirmées comprennent :

- `dcbst` décodé/dispatché mais émis en no-op, sans même calcul d'EA ;
- `frsqrte`, `vrefp` et `vrsqrtefp` calculés par math hôte exacte ;
- `lwarx/ldarx` mémorisant seulement la valeur et store-conditionnel sans
  adresse/granule/génération ;
- `sync`, `lwsync`, `eieio` sans fence hôte ou publication MMIO ;
- `vmsum4fp128` via `DPPS`, en contradiction avec le résultat du contrôle
  SLEIGH PAL actuel ; ce contrôle n'est lui-même pas une preuve console ;
- VSCR.SAT non mis à jour par les packs saturants ;
- FPSCR réduit et exceptions/FPRF/SNaN incomplets ;
- arguments HLE 64 bits tronqués et ordinal mixte FP/entier incorrect dans la
  couche générique ;
- FMA scalar de l'ancien SDK 0.7.1 décomposé en `a*b+c` alors que 0.9 utilise
  `std::fma` ;
- MFTB à 50 MHz dans ReXGlue, tandis qu'Unleashed annonce 49,875 MHz : dessin
  d'horloge utile, constante encore non qualifiée.

Le pseudo-test `frsqrte` est inactif : ses directives sont écrites `# _` alors
que le parseur attend `#_`. Même activée, la suite ne teste ni atomiques,
barrières, MFTB, FPSCR, `vrefp` ni `vrsqrtefp`.

### Neuf anciens trous XenonRecomp AC6

| Instruction | Sites PAL | ReXGlue 0.9 | Statut |
|---|---:|---|---|
| `dcbst` | `821D8DD0`, `821D9210`, `821D9240`, `821D9270` | no-op | `divergent` |
| `vpkswss` | `82208F60`, `82208FA4` | lanes pack/clamp plausibles, pas VSCR.SAT | lanes provisoires, instruction complète divergente |
| `mulhdu` | `823D9CC4` | haut du produit 128 bits | `provisional-rexglue` après tests indépendants |
| `frsqrte` | `823E16A8`, `823E17D8` | exact `1/sqrt` | `divergent` |

Ces neuf sites ne définissent pas le cône M01 total. Le census connu contient
aussi des centaines de `vrefp`/`vrsqrtefp` et des dizaines d'atomiques ou
barrières. La trace d'exécution doit dire lesquels sont réellement atteints.

## Remplacer la plupart des micro-exécutions

La nouvelle stratégie est : modèle indépendant d'abord, ReXGlue comme contrôle
provisoire, micro-exécution seulement pour une ambiguïté matérielle atteinte.

Peuvent être qualifiés sans micro-exécution retail :

- arithmétique entière comme `mulhdu`, avec modèle bigint et milliers de
  vecteurs seedés ;
- lanes de packs/clamps, ordre A/B, alias et registres `v0/v31/v32/v127` ;
- endian, adresses effectives `RA=0`, pointeurs invités et placement ABI ;
- copies bit à bit, y compris NaN/sNaN/sign-zero, par `memcpy` et canaris ;
- parsing, tiling/endian et formats par fixtures synthétiques et image positive ;
- horloge injectée, monotonie, rollover et replay ;
- invariants atomiques négatifs, notamment refus d'un store à autre adresse.

Restent des candidats légitimes à une preuve console/oracle/microexec bornée :

- tables d'estimation réciproque et suites Newton ;
- NaN payloads, SNaN, FPSCR/FPRF/exceptions et denorm Xenon ;
- granule de réservation et conflits interthreads ;
- publication `dcbst` vers un consommateur GPU non cohérent ;
- ordre exact des barrières avec MMIO ;
- dot products/sommes VMX128 en cas de résultat discriminant ;
- fréquence timebase si elle affecte un événement M01.

Cela respecte la politique static-first : les modèles synthétiques ferment les
sémantiques standards ; seuls les comportements Xbox non dérivables et
effectivement atteints coûtent une exécution spécialisée.

## Plan PAL/M01 requalifié

### R1 — figer l'oracle effectivement exécuté

Conserver le PAL légal et le checkout AC6 `dcd41b` comme première lane. Sceller
commit AC6, arbre ReXGlue 0.7.1, config, binaire, XEX, cache retail et options.
Ne pas attribuer les corrections 0.9 à ce binaire. Une montée de SDK est une
expérience séparée avec diff sémantique et nouveau manifeste.

### R2 — replay poll-exact depuis le boot

Instrumenter `XamInputGetState_entry`, pas seulement le driver fusionné.
Journaliser chaque poll, résultat, état XInput, caller LR, user/flags/pointeur,
marqueur frame et télémétrie present. Au replay, prévalider tout le fichier,
bypasser le périphérique, refuser EOF/restes/divergence et ne jamais fallback
sur l'input physique.

La cadence globale boot→M01 reste `unqualified`; une fenêtre M01 stable reçoit
un census mesuré et une projection 30→60 explicite si nécessaire. Le reçu de
projection doit empêcher un double hold dans le builder natif.

### R3 — census du cône M01

Sur deux replays identiques, produire :

- instructions/builders effectivement atteints ;
- imports HLE atteints et stubs ;
- appels indirects et hooks ;
- événements D3D/PM4, registres et opcodes ;
- shaders/vertex fetch/texture fetch/formats ;
- uploads, endian/tiling, EDRAM, resolves/readbacks ;
- XMA/ASF, VFS et événements temporels.

La moindre instruction absente, stub atteint ou divergence connue devient un
record `divergent` avec premier poll/tick/callsite. Le reste peut alimenter le
port manuscrit sous statut `provisional-rexglue`.

### R4 — implémenter sans dépendance oracle

Le produit consomme seulement le cache retail v2 et ses propres structures :
session, monde, `DrawPacket`, audio/vidéo, input et sauvegarde. Les traces
ReXGlue sont des fixtures/recettes externes ; aucun header, symbole, code
généré, backend ou octet retail n'entre dans le paquet.

### R5 — qualifier tardivement le cône atteint

Quand M01 est jouable de bout en bout, regrouper les dépendances par domaine et
les promouvoir : bytes PAL + Ghidra canonique + ABI/dataflow + test natif ;
oracle exécuté seulement si le modèle laisse une ambiguïté matérielle. Cette
étape ferme les six lanes et les gates, elle ne bloque plus le bring-up.

## Frontières de publication

Avant la preview, il reste interdit de :

- fermer une lane avec une preuve seulement `provisional-rexglue` ;
- embarquer ReXGlue, Xenia, code généré ou assets retail ;
- conserver un fallback renderer émulé/CPU interactif ;
- utiliser un nom généré comme sémantique ;
- joindre une adresse US au PAL ;
- ignorer un stub ou une divergence parce que l'image « semble correcte ».

## Validation et risques résiduels

Les checkouts, commits, arbres, licences, catalogues locaux, builders CPU,
tests/CI, runtimes, command processors, caches texture, translators shader et
rapports PAL existants ont été inspectés de façon ciblée. rexdex n'a pas été
buildé : son chemin est VS2017/MSVC/DX11 et ses défauts bloquants sont visibles
dans les sources actives. Aucun octet retail ni sortie générée n'a été ajouté.

Le risque dominant n'est plus « comprendre toute la Xbox avant M01 », mais
« croire qu'une révision ReXGlue en représente une autre ». Les manifests et
le diff sémantique par révision sont donc la garde centrale de cette route
accélérée.

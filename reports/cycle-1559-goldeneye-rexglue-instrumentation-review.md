# Cycle 1559 — audit GoldenEye, instrumentation et paquet ReXGlue

Date de qualification : 12 août 2026.

## Résultat

GoldenEye-Recomp apporte un bon vocabulaire d'observation du GPU invité :
compteur de swaps du command processor, pointeurs lecture/écriture du ring,
compteurs submit/presented, fences, polls invités et états des threads. Cette
forme peut accélérer le diagnostic M01 si elle devient une trace bornée et
strictement passive.

Le code public ne constitue cependant pas un oracle sémantique. Son
« watchdog » modifie les sémaphores, horloges, pointeurs de ring et bits de
skip du guest pour sortir des blocages. Le chemin input injecte après le poll
XAM dans une adresse globale spécifique au jeu et le mouse-look écrit
directement la caméra. Le réseau annoncé n'est pas présent dans les sources
publiques. Enfin, douze grands tableaux d'octets communautaires sont compilés
dans l'exécutable sans provenance ou licence par fichier, tandis qu'une
ancienne release est publiquement signalée comme ayant fourni un XEX.

Conclusion de gate : **aucune lane M01 n'est fermée**. Les seules reprises
admissibles sont des formes de télémétrie et de tests, réécrites sans effets de
bord et requalifiées sur le PAL AC6.

## Périmètre, identité et reproductibilité

Audit en lecture seule de
[SunJaycy/GoldenEye-Recomp](https://github.com/SunJaycy/GoldenEye-Recomp/tree/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d),
sans téléchargement de release ni contenu de jeu.

| Élément | Valeur vérifiée |
| --- | --- |
| commit HEAD/main et tag `Goldeneye1.2.4` | `fdee4d1f750aff4c3b5c6ba3d60f20281c21447d` |
| tree Git | `959ca05b2f3bd8ea88d04105c0a146bac2a848f2` |
| historique public | 20 commits, du 12 au 18 juin 2026 |
| licence racine | Unlicense |
| fichiers suivis | 31 |
| code C++/headers | 3 304 lignes, hors tableaux d'octets |
| hooks MIDASM déclarés | 40 |
| SDK déclaré | `0.8.0.0`, sans commit, sous-module ou archive scellée |
| XEX déclaré | `assets/default.xex`, sans SHA-256, Title ID, Media ID ni version |
| tests et CI publics | aucun |

Le manifeste ne contient que la version logique du SDK et le chemin du XEX :
[ge_manifest.toml, lignes 1–13](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/ge_manifest.toml#L1-L13).
Le runtime et le code généré sont absents ; le CMake dépend directement de
`generated/rexglue.cmake` :
[CMakeLists.txt, lignes 7–41](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/CMakeLists.txt#L7-L41).

La configuration Linux publique n'est pas une preuve de build. Le preset
exige `clang-20`, absent du runner ; en le remplaçant explicitement par Clang
21, la configuration s'arrête sur le `generated/rexglue.cmake` absent. Même
avec une génération locale, `ge_hooks.cpp` inclut sans garde `windows.h` et
`shellapi.h` :
[ge_hooks.cpp, lignes 27–55](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/src/ge_hooks.cpp#L27-L55),
alors que les presets annoncent Linux amd64 et arm64 :
[CMakePresets.json, lignes 21–52](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/CMakePresets.json#L21-L52).
Le build Linux est donc `documented-unmatched` au HEAD audité.

## Grille de qualification

| Classe | Éléments GoldenEye |
| --- | --- |
| **provisional-rexglue** | noms et forme des compteurs GPU, contrat adresse/phase/registres MIDASM, séparation lifecycle UI, inventaire metadata-only de blobs |
| **retail-qualified** | **aucun** |
| **divergent** | horloge guest injectée, sémaphore libéré par l'hôte, frame forcée complète, bit skip forcé, pointeur de ring réécrit, mouse-look direct en RAM, patches CE |
| **documented-unmatched** | réseau/serveur public absent, passe GPU de color grading absente, build Linux non reconstructible, identité exacte du XEX absente |

## Télémétrie GPU : bonne surface, mauvais mélange

### Surface d'observation intéressante

Le dépôt expose un petit adaptateur vers le command processor ReXGlue pour
lire `read_ptr_index`, `write_ptr_index` et le compteur de swaps :
[ge_hooks.cpp, lignes 59–90](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/src/ge_hooks.cpp#L59-L90).
Le watchdog rapproche ensuite :

- ring `rpi/wpi` ;
- compteur de present du CP ;
- nombre de polls guest ;
- submit, completed, target et presented guest ;
- compteurs vblank et fences ;
- LR, CTR, dernière cible indirecte et état de threads guest.

Le census et le diagnostic sont visibles dans
[ge_hooks.cpp, lignes 134–218](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/src/ge_hooks.cpp#L134-L218)
et le snapshot des threads dans
[lignes 234–305](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/src/ge_hooks.cpp#L234-L305).

Cette décomposition est utile pour AC6 : elle distingue producteur guest,
consommation du ring, swap, fence et présentation. Elle ne doit toutefois être
qu'une télémétrie. Un ring drainé ne prouve ni la visibilité de l'image, ni le
retrait d'une fence, ni l'identité de la frame observée.

### L'observateur modifie l'expérience

Le commentaire annonce « zero gameplay effect », mais le thread écrit zéro
dans le sémaphore guest après deux échantillons de 250 ms pour libérer un
`WAIT_REG_MEM` :
[ge_hooks.cpp, lignes 123–173](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/src/ge_hooks.cpp#L123-L173).

Le hook `ge_dbg_now` remplace aussi une horloge guest par
`REX_QUERY_TIMEBASE`, puis :

- considère la frame dessinée si le compteur CP change **ou** si `rpi == wpi` ;
- après environ 80 ms hôte, force le bit guest `device+10941 |= 2` ;
- marque `presented := submit` ;
- réécrit le pointeur de lecture dans le bloc guest.

Ces mutations sont explicites dans
[ge_hooks.cpp, lignes 399–448](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/src/ge_hooks.cpp#L399-L448)
et
[lignes 450–506](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/src/ge_hooks.cpp#L450-L506).
Il s'agit d'un mécanisme de continuité de bring-up, pas d'un oracle retail.

Le watchdog et la souris démarrent en outre des threads détachés, sans stop,
join ou ownership de shutdown :
[ge_hooks.cpp, lignes 390–395](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/src/ge_hooks.cpp#L390-L395)
et
[lignes 814–851](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/src/ge_hooks.cpp#L814-L851).
La lecture concurrente des contextes PPC et de la RAM guest n'est accompagnée
d'aucun snapshot ou synchronisation démontrée.

### Contrat AC6 à retenir

Un futur `OracleGpuTelemetry` M01 peut enregistrer, à des seams déjà
qualifiés, un tuple borné :

`marker, guest_thread, guest_lr, present_index, ring_rptr, ring_wptr,
submit_id, completed_id, fence_read, fence_write`.

Le test essentiel n'est pas seulement le format : le composant d'observation
ne doit exposer aucune capacité d'écriture guest et aucune horloge de pilotage.
Les récupérations restent dans un mode de bring-up séparé, marqué divergent,
et leur activation rend toute trace impropre à la comparaison.

## Input : seam post-XAM et caméra directe

Le hook `ge_inject_keyboard` se place après que le jeu a rempli un buffer
interne slot 0, puis écrit boutons, triggers et sticks à l'adresse fixe
`0x830C8B9C` :
[ge_config.toml, lignes 115–124](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/ge_config.toml#L115-L124)
et
[ge_hooks.cpp, lignes 1099–1109](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/src/ge_hooks.cpp#L1099-L1109).
Il ne capture donc ni les polls XAM qui échouent, ni user/flags/LR/pointeur,
ni les appels hors de ce consommateur particulier.

Au premier poll, le même hook applique aussi toutes les modifications de
données CE :
[ge_hooks.cpp, lignes 1178–1195](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/src/ge_hooks.cpp#L1178-L1195).
Le seam mélange ainsi entrée et mutation du monde.

La souris est encore plus éloignée d'un replay normalisé. Un thread Windows
accumule les deltas `WM_INPUT`, consommés selon le nombre de polls hôte ; le
code écrit directement menu, caméra, crosshair, arme et options d'auto-aim en
RAM guest :
[ge_hooks.cpp, lignes 786–855](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/src/ge_hooks.cpp#L786-L855)
et
[lignes 918–1028](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/src/ge_hooks.cpp#L918-L1028).

Conséquence AC6 : conserver le seam poll-exact dans
`XamInputGetState_entry`, avant toute fusion spécifique au jeu. Le replay
enregistre le résultat XAM et l'état XInput, puis seulement le natif projette
vers ses entrées normalisées. Aucun champ caméra ou gameplay n'est rejoué.

## Hooks et reconstruction de trous

Le dépôt est un bon cas négatif pour les frontières de fonctions. Huit
branches visent une plage annoncée zéro dans le XEX statique ; le projet
reconstruit leurs effets depuis des fragments et continuations identifiés dans
IDA :
[ge_hooks.cpp, lignes 1–10](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/src/ge_hooks.cpp#L1-L10)
et
[ge_config.toml, lignes 1–48](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/ge_config.toml#L1-L48).

Le registre adresse/phase/registres/jump/return de MIDASM est une forme utile
pour documenter une preuve. Mais aucune identité XEX n'est scellée, aucun
listing de bytes n'est joint et les noms/commentaires générés ne prouvent pas
les huit sémantiques. Pour AC6, un tel hook doit lier projet Ghidra canonique,
SHA XEX, module, plage `.pdata`, bytes exacts, ABI, gardes, lectures/écritures,
appelants et contrôle positif. Sans cela il reste `provisional-rexglue`.

## Réseau annoncé, transport absent

Le README annonce un multijoueur en ligne et un serveur :
[README, lignes 25–42](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/README.md#L25-L42).
Le menu ne fait qu'éditer quatre cvars puis redémarrer le processus :
[ge_menu.cpp, lignes 757–818](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/src/ge_menu.cpp#L757-L818).

Le dépôt ne contient ni client, serveur, socket, protocole, dépendance réseau,
test, script de déploiement annoncé, ni définition publique des cvars
`ge_online_*`. Les hooks « network » présents ne font que modifier des règles
du jeu lorsque le byte `0x830CAEA0` est actif :
[ge_hooks.cpp, lignes 1306–1357](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/src/ge_hooks.cpp#L1306-L1357).
La capacité réseau est donc `documented-unmatched`, probablement portée par un
runtime/release non publié. Elle ne fournit aucun enseignement réutilisable au
shell hors-ligne AC6.

## Octets compilés et audit de paquet

Le README affirme que le dépôt ne contient aucun asset ou code du jeu :
[README, lignes 62–66](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/README.md#L62-L66).
Le `.gitignore` exclut bien `generated/` et `assets/` :
[.gitignore, lignes 13–21](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/.gitignore#L13-L21).

Mais `src/ce_patches` contient douze tableaux d'octets, 3 710 970 octets de
source et 742 040 octets décodés. Les quatre plus gros sont :

| tableau | octets décodés | SHA-256 décodé |
| --- | ---: | --- |
| `siloicbmview` | 318 896 | `c58f7a66cc69216bb1cd268bf8817138f030738cd4c0f5e15cd8405865d078f3` |
| `frigatebg_fixportals` | 176 560 | `8f9b090b6b32013ce2dfc7f3ba1337f69bd06ffe23dc9f0dfacbbff34de7a110` |
| `aztecshuttlearea` | 128 912 | `cdebc065c4a02fc989dd0dbe1cd906e94ed906611954f2d7c188cecd614e4d3b` |
| `surfacebg_portaladditions` | 113 056 | `7f7dda043b7e8bef2aec28ed9f68536177cf24a13a956a0461f3362d018e4306` |

Le code les décrit comme des blobs BeanTools, les copie tels quels vers des
adresses guest et affirme une correspondance 1:1 avec un `finalizer.c` qui
n'est pas dans le dépôt :
[ge_ce_patches.cpp, lignes 1–32](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/src/ge_ce_patches.cpp#L1-L32)
et
[lignes 260–285](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/src/ge_ce_patches.cpp#L260-L285).
Il n'existe ni attribution, ni provenance, ni licence par tableau. L'audit ne
peut pas décider si chaque octet est original, transformé ou dérivé du jeu ;
il les classe donc `provenance-unresolved`, jamais redistribuables par
transitivité de l'Unlicense racine.

Le risque de paquet est concret. L'issue publique
[#34](https://github.com/SunJaycy/GoldenEye-Recomp/issues/34) signale que la
release 1.2 contenait `default.xex`. Dans
[#29](https://github.com/SunJaycy/GoldenEye-Recomp/issues/29#issuecomment-4717723186),
le mainteneur explique que fournir ce XEX était intentionnel pour faciliter la
sélection de la bonne version. L'archive n'a pas été téléchargée pendant cet
audit. Les métadonnées GitHub seules donnent :

| release | asset | taille | digest GitHub |
| --- | --- | ---: | --- |
| [1.2](https://github.com/SunJaycy/GoldenEye-Recomp/releases/tag/Goldeneye1.2) | `GoldenEye-Release-Win.rar` | 15 589 101 | `sha256:e3a0df1534aa1fb66bdd7e3713248c4764a81da104663023c5a02bde300dacbf` |
| [1.2.4](https://github.com/SunJaycy/GoldenEye-Recomp/releases/tag/Goldeneye1.2.4) | `GoldenEye-Recomp-Win.rar` | 9 588 424 | `sha256:78793e2e4651709de0ae9e929be03096c23c50e6ba814c5ef4e8137fb557e635` |

Pour AC6, le contrôle publication doit donc inspecter récursivement le TGZ
installé, les bibliothèques, ressources, fichiers générés et gros littéraux
encodés. Vérifier seulement `git ls-files`, `.gitignore` ou le staging ne
suffit pas. Toute longue initialisation d'octets exige une provenance et une
allowlist explicites ; le paquet final est comparé aux SHA et sous-chaînes
bornées du XEX, du cache importé et des blobs retail.

## Autres écarts utiles

- Les effets de color grading sont annoncés comme une passe D3D12 du swap,
  mais le dépôt ne contient que l'overlay ImGui vignette/scanlines :
  [ge_postfx.cpp, lignes 1–34](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/src/ge_postfx.cpp#L1-L34)
  et
  [lignes 128–158](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/src/ge_postfx.cpp#L128-L158).
  La passe GPU est `documented-unmatched` dans le runtime absent.
- Le menu est détruit proprement, mais les deux threads auxiliaires ne le sont
  pas : le lifecycle UI n'est pas un modèle de lifecycle runtime complet :
  [ge_app.h, lignes 68–93](https://github.com/SunJaycy/GoldenEye-Recomp/blob/fdee4d1f750aff4c3b5c6ba3d60f20281c21447d/src/ge_app.h#L68-L93).
- Les patches communautaires sont appliqués au premier poll input sans
  identité du XEX et sans contrôle des bytes avant écriture. Un mauvais XEX
  transforme les adresses fixes en corruption silencieuse.
- Les changements de gameplay CE, d'auto-aim, de fog, de near clip et de
  multijoueur sont des options modernes ; ils ne doivent jamais être actifs
  dans le profil retail ou les gates de fidélité AC6.

## Tranches recommandées pour AC6 M01

1. Ajouter une télémétrie oracle passive et bornée aux seams ReXGlue déjà
   qualifiés. Elle scelle identité binaire/configuration, ordre des événements
   et tuple GPU, sans thread détaché, scan mémoire concurrent ni écriture guest.
2. Ajouter un drapeau irrécusable `oracle_mutation_active`. Toute récupération,
   skip, horloge injectée ou patch guest rend l'artefact inéligible aux gates.
3. Conserver le replay à `XamInputGetState_entry`; bannir les hooks post-buffer
   et les écritures caméra du protocole de validation.
4. Pour chaque hook, sceller `{XEX SHA, module, address, phase, byte range,
   byte SHA, ABI, guards, reads, writes, callers}`. Une adresse seule ne suffit
   pas.
5. Ajouter à l'audit du TGZ : extraction récursive bornée, liste de tous les
   fichiers, détection XEX/PAC, comparaison de hashes, scan des grands tableaux
   d'octets et preuve de provenance/licence. Exécuter le même audit sur
   l'installation relogeable.
6. Ne reprendre aucun code ou tableau GoldenEye. La télémétrie proposée est
   une réécriture manuscrite minimale ; les adresses, règles et patches sont
   spécifiques à un autre titre et non qualifiés.

## Validations de l'audit

- commit, tree, tag, remote et historique vérifiés par Git ;
- 31 fichiers suivis inventoriés, aucun sous-module ;
- 40 hooks MIDASM recensés ;
- douze tableaux décodés localement sans les publier : 742 040 octets, hash de
  manifeste nom/taille/contenu
  `05522e25100cd90828449b433eef987b10947470b23b7c7e80daa697a1cb8639` ;
- métadonnées des deux releases lues via l'API GitHub, archives non
  téléchargées ;
- configuration CMake Linux testée jusqu'à la dépendance générée absente ;
- aucun contenu retail, code généré ou runtime externe utilisé.

## Risques résiduels

- Le SDK exact et le transport réseau pourraient exister dans une distribution
  privée ou une release ; ils ne sont pas auditables depuis le source public.
- Les tableaux CE n'ont pas été comparés à un XEX ou asset GoldenEye ; leur
  classification reste provenance non résolue, pas une affirmation juridique.
- Aucune release n'a été extraite : l'état exact de 1.2 et 1.2.4 n'est pas
  revalidé localement. Les issues publiques suffisent uniquement à imposer la
  garde de paquet AC6.

# Cycle 1554 — audit du runtime Re-Cherry / Lollipop Chainsaw

## Verdict

Le dépôt public Lollipop Chainsaw n'expose **aucun fork ni delta du runtime
ReXGlue**. Il contient une mince couche titre, une configuration de codegen et
un exécutable Windows, mais ni SDK épinglé, ni C++ généré, ni implémentation
VFS/XAM/XMA/Xenos. Ces sous-systèmes ne peuvent donc pas être qualifiés depuis
ce dépôt.

La seule technique utile à AC6 M01 est le contrat minimal d'un hook MIDASM :
adresse invitée, phase avant/après et registres explicitement capturés. C'est
une confirmation `provisional-rexglue` du mécanisme d'instrumentation, pas une
preuve sémantique. AC6 conserve sa version plus stricte, liée au SHA-256 du XEX
PAL, au hash des octets de l'instruction, au rôle du marqueur et au reçu de
projection. Aucun code Re-Cherry ne doit être repris.

L'audit ne ferme aucune lane M01. Il apporte surtout des contre-exemples à
garder hors de la preview : assets retail placés dans un arbre sans vrai
`.gitignore`, fichiers de jeu modifiés en place, mesure de cadence par horloge
hôte, bypass UI localisé non câblé, build non reproductible et paquet sans
licence explicite.

## Provenance et frontière examinée

| Élément | Pin vérifié le 12 août 2026 |
|---|---|
| dépôt | [`MaxDeadBear/Re-Cherry`](https://github.com/MaxDeadBear/Re-Cherry) |
| branche `main` | `4f8f82028c02e25a32402b4de96f9c23e2f3b7c5` |
| tag `release` | même commit `4f8f82028c02e25a32402b4de96f9c23e2f3b7c5` |
| arbre Git | `20d66ffa0ec7df6de008fcdc6a1777dc710fd410` |
| date du commit | `2026-04-01T06:54:16Z` |
| remote | `https://github.com/MaxDeadBear/Re-Cherry.git` |

Le HEAD et le tag ont été recoupés avec `git ls-remote`. L'arbre local était
propre. Il contient seulement 18 fichiers suivis : 588 lignes de manifeste,
CMake et code hôte, plus un exécutable Windows de 91 054 080 octets. Aucun
octet retail, C++ généré ou exécutable n'a été ouvert pendant cet audit.

Empreintes SHA-256 des sources principales :

- configuration codegen :
  `924eb2a2fa860dd5a0e2682414abe81c42b6fdc6ad5dda5fc4bb0ed878dbec6e` ;
- hooks FPS :
  `29d104a9c10f25f979d635983c14d31338ebd3c9ca77dc9a158068a01565928c` ;
- application hôte :
  `86d976061f5c172c25a7cfe1d5d9b298c84ef43748d4d2a3809b621d997784f1` ;
- gestionnaire de costumes :
  `a2f2ccf86e5d1227c83a2d1c2b3bbc2e9a65285d999f86d132e40140012634ce` ;
- bypass Xbox Live :
  `917fef07dfdb7fd0386e30a9ebd607c97b2ba44dd13109435bfe75741d9afb6d` ;
- CMake racine :
  `fee7f525202851be8a92d5fa95f2e05775b79f4c3d638705d3d38064fb5c00ca`.

La configuration ne scelle ni SHA-256 du XEX, ni Title ID, ni Media ID, ni
version XEX ; elle ne donne qu'un chemin `assets/default.xex`. Les 39 adresses
de fonctions et leurs commentaires sémantiques ne sont donc pas une source
qualifiée pour AC6. Aucun résultat ne peut être transposé entre ce XEX inconnu
et le PAL AC6.

## Ce que le dépôt ajoute réellement

Le [CMake racine](https://github.com/MaxDeadBear/Re-Cherry/blob/4f8f82028c02e25a32402b4de96f9c23e2f3b7c5/CMakeLists.txt#L13-L28)
inclut un `generated/rexglue.cmake` absent de Git et appelle
`rexglue_setup_target`. Il n'existe ni submodule, ni lockfile, ni version du
SDK, ni fork embarqué. Le
[README](https://github.com/MaxDeadBear/Re-Cherry/blob/4f8f82028c02e25a32402b4de96f9c23e2f3b7c5/README.md#L5-L21)
demande simplement d'installer le SDK courant puis de lancer `rexglue codegen`.
Un checkout ne permet donc pas de reconstruire l'exécutable publié avec une
identité ReXGlue vérifiable.

La couche titre publique se limite à :

- une sous-classe `rex::ReXApp`, un dialogue ImGui et des CVar ;
- 39 départs de fonctions déclarés sans corps public ;
- deux hooks MIDASM ;
- un remplacement de costumes par copies de fichiers ;
- un source de bypass Xbox Live qui n'est pas relié à la cible.

VFS, sauvegardes, entrée XAM, audio XMA, vidéos, command processor Xenos,
shaders, textures et synchronisation GPU appartiennent tous au SDK/généré
absent. Les attribuer à Re-Cherry serait confondre documentation et source
auditée.

## Hooks et cadence

La
[configuration](https://github.com/MaxDeadBear/Re-Cherry/blob/4f8f82028c02e25a32402b4de96f9c23e2f3b7c5/re_cherry_config.toml#L52-L62)
déclare :

- `0x829EAA14`, hook `fps`, registre `r11`, avant l'instruction ;
- `0x829EBA3C`, hook `counter`, sans registre, avant l'instruction.

Ce format adresse/registres/phase est un patron utile pour une instrumentation
bornée. En revanche, son usage concret est `divergent` :

- [`fps`](https://github.com/MaxDeadBear/Re-Cherry/blob/4f8f82028c02e25a32402b4de96f9c23e2f3b7c5/src/hooks.cpp#L16-L24)
  remplace `r11` par `1` ou `2` selon une CVar 30/60 FPS et modifie donc le
  comportement invité ;
- [`counter`](https://github.com/MaxDeadBear/Re-Cherry/blob/4f8f82028c02e25a32402b4de96f9c23e2f3b7c5/src/hooks.cpp#L26-L35)
  mesure une seule durée de frame avec `system_clock`, puis n'en publie qu'une
  toutes les 60 invocations ; ce n'est ni une moyenne, ni une horloge monotone,
  ni un compteur invité ;
- `fpsCount` est un `double` global non atomique, lu par l'UI sans contrat de
  thread public ;
- aucun test ne prouve la fréquence d'appel, l'instruction remplacée ou la
  stabilité du hook.

Décision AC6 : n'utiliser ce dépôt que comme confirmation de la forme du hook.
La cadence du replay reste dérivée d'un census runtime borné et scellé ; une
chaîne `runtime_census` auto-déclarée ou une horloge murale ne constitue pas
une preuve.

## VFS, assets, sauvegardes et atomicité

Le
[README](https://github.com/MaxDeadBear/Re-Cherry/blob/4f8f82028c02e25a32402b4de96f9c23e2f3b7c5/README.md#L11-L29)
demande de placer `default.xex` et l'arbre ISO dans `assets`, puis de recopier
ce répertoire à côté de l'exécutable. Or les trois fichiers censés protéger
`assets`, `assets/SkinMods` et `generated` s'appellent littéralement
[`gitignore.txt`](https://github.com/MaxDeadBear/Re-Cherry/blob/4f8f82028c02e25a32402b4de96f9c23e2f3b7c5/assets/gitignore.txt#L1-L2),
pas `.gitignore`. Leurs règles `*` ne sont jamais interprétées par Git. Suivre
les instructions rend donc tous les actifs retail et tout le code généré
visibles comme fichiers non suivis, avec un risque direct de publication
accidentelle.

Le
[gestionnaire de costumes](https://github.com/MaxDeadBear/Re-Cherry/blob/4f8f82028c02e25a32402b4de96f9c23e2f3b7c5/src/costume_switcher.cpp#L50-L114)
copie un fichier retail vers `.bak`, écrase ensuite la destination, puis
restaure par un autre écrasement et supprime le backup. Il n'y a ni fichier
temporaire, ni `fsync`, ni rename atomique, ni hash de l'original, ni protection
contre une interruption ou deux processus concurrents. Les erreurs de
`from_chars` sont ignorées. Aucun code de sauvegarde jeu n'est public.

Décision AC6 : route entièrement `divergent`. Conserver l'import/cache retail
v2 atomique, immuable après publication, sous XDG ; ne jamais modifier ou
relire les PAC pendant `play`, et garder l'audit de paquet qui refuse tout
octet retail.

## XAM, entrée et shell hors ligne

Il n'existe aucune implémentation publique d'entrée, de `GetState`, de
`GetKeystroke`, de packet number, de hotplug ou de replay. Les deux commentaires
`InputRingBuf_*` du manifeste nomment des fonctions sans publier leurs corps et
sans identité XEX. Ils sont `documented-unmatched`, pas une analyse sémantique.

Le
[bypass Xbox Live](https://github.com/MaxDeadBear/Re-Cherry/blob/4f8f82028c02e25a32402b4de96f9c23e2f3b7c5/src/xbox_live_bypass.cpp#L12-L102)
cherche quelques chaînes anglaises, force temporairement le mode `headless` et
sélectionne le bouton d'indice 1. Même comme prototype, il diverge des langues
PAL et retire une action de la timeline joueur. Surtout,
`src/xbox_live_bypass.cpp` n'est pas dans `RECHERRY_SOURCES`, son header n'est
inclus par aucun autre fichier et l'adresse `0x82BF4288` n'apparaît pas dans le
manifeste. À ce HEAD, l'affirmation du message de commit « separated xbox live
prompt skip » correspond à du source dormant, pas à un chemin exécutable
vérifiable.

Décision AC6 : ne reprendre ni recherche textuelle, ni auto-validation UI. Le
frontend M01 choisit explicitement le hors-ligne ; toute action utile reste une
entrée normalisée dans le replay poll-exact.

## XMA, médias et synchronisation A/V

Le dépôt ne contient aucun décodeur, lecteur ASF, pipeline XMA, horloge média,
queue audio ou gestion de sous-titres. Tout résultat éventuel provient du SDK
non épinglé. L'[issue 3](https://github.com/MaxDeadBear/Re-Cherry/issues/3),
ouverte le 12 août 2026, signale en outre une cinématique plus lente que sa
voix. C'est un symptôme public non reproduit ici, pas une preuve de cause, mais
il exclut Re-Cherry comme oracle de synchronisation.

Décision AC6 : aucun élément réutilisable. Garder les flux FFmpeg bornés, la
timeline M01, les cues à ±20 ms et l'amplitude à ±1 dB comme gates séparées.

## Renderer, Xenos et stabilité

Aucun renderer, command processor, shader, décodeur de texture, modèle EDRAM,
fence ou test image n'est public dans ce dépôt. Le README liste le tearing
comme problème courant. L'[issue 4](https://github.com/MaxDeadBear/Re-Cherry/issues/4),
ouverte le 12 août 2026, décrit des pertes du périphérique graphique après
quelques minutes ; là encore, c'est un signal utilisateur, sans trace qualifiée
ni correctif source.

Le hook 60 FPS ne démontre ni découplage simulation/présentation, ni invariance
du gameplay. Il ne peut pas informer la lane Vulkan native AC6, les `DrawPacket`
M01, BC3 tiled/endian, les mips/cubemaps ou le contrôle SSIM.

Décision AC6 : renderer entièrement `documented-unmatched`. Conserver Vulkan
validation, les tests de recréation de swapchain et les contrôles image positifs
comme conditions de publication.

## Linux, build, paquetage et licence

Les
[presets](https://github.com/MaxDeadBear/Re-Cherry/blob/4f8f82028c02e25a32402b4de96f9c23e2f3b7c5/CMakePresets.json#L22-L80)
prévoient Linux AMD64 avec `clang-20`, mais le dépôt ne fournit pas le fichier
CMake généré, ne fixe pas le SDK et ne possède ni CI ni test. La release
publique [`release`](https://github.com/MaxDeadBear/Re-Cherry/releases/tag/release)
contient seulement un exécutable Windows x64 et un TOML. Il n'existe aucune
règle `install`, aucun TGZ Linux, aucune vérification de relogeabilité, aucune
SBOM et aucun audit d'assets.

Le dépôt ne contient ni `LICENSE`, ni `COPYING`, ni `NOTICE`; l'API GitHub ne
détecte aucune licence. Même les quelques sources hôte ne doivent donc pas être
copiées dans AC6. La licence éventuelle d'un SDK ReXGlue externe ne couvre pas
automatiquement ce dépôt titre.

Le binaire suivi et la release ne peuvent pas être reliés de façon
reproductible à un commit SDK, au C++ généré, au compilateur et aux options de
link. Ils ne servent ni de source, ni d'oracle déterministe, ni de modèle de
paquet propre.

## Tests et qualification

L'arbre ne contient aucun test, `CTest`, workflow `.github`, sanitizer, fuzz,
validation GPU ou reçu de build. Un build propre n'a pas été tenté : il
nécessiterait de choisir arbitrairement un SDK externe non épinglé et de fournir
un XEX retail, ce qui ne qualifierait pas le commit audité.

Les contrôles statiques de cet audit ont confirmé :

- HEAD, arbre et tag exacts ;
- absence de submodule, lockfile, SDK vendored et manifeste d'identité XEX ;
- absence des sous-systèmes VFS/XAM-input/XMA/Xenos dans les sources suivies ;
- omission du bypass Xbox Live dans la cible CMake ;
- absence de vrai `.gitignore`, de tests, de CI et de licence ;
- disponibilité HTTP des liens sources épinglés utilisés ci-dessus.

## Classement AC6 M01

| Élément | Classe | Décision |
|---|---|---|
| schéma hook adresse/registres/phase | `provisional-rexglue` | retenir le concept, avec identité/code hash/reçu AC6 |
| `ReXApp`, CVar, ImGui, `game_data_root` | `provisional-rexglue` | plomberie de bring-up seulement ; aucun gate M01 |
| 39 commentaires de fonctions | `documented-unmatched` | ne pas inférer de sémantique ni de boundary AC6 |
| SDK, VFS, saves, XAM input, XMA/ASF, Xenos | `documented-unmatched` | source absente et version inconnue |
| presets Linux | `documented-unmatched` | aucun build/release Linux vérifiable |
| bypass Xbox Live | `divergent` et `documented-unmatched` | heuristique localisée et TU non câblé |
| hook 30/60 FPS et compteur mural | `divergent` | exclure des replays et gates retail |
| mutation costumes / assets à côté du binaire | `divergent` | conserver import XDG atomique et cache immuable |
| synchronisation A/V et stabilité GPU | `retail-needed` | qualifier sur AC6 PAL, pas sur Re-Cherry |
| mécanisme `retail-qualified` | **aucun** | aucune lane fermée |

## Suite bornée

Cet audit ne justifie aucun port de code. Il renforce quatre gardes déjà dans la
route M01 : identité PAL et hash d'instruction pour chaque hook, replay
poll-exact scellé, package sans retail avec vrais ignore/audits, et validations
A/V plus Vulkan indépendantes. Re-Cherry peut rester dans le catalogue comme
exemple de bring-up ReXGlue, pas comme source de fidélité retail.

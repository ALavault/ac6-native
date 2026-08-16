# Domaine 1 — `ac6-demo-native` : identité, import et VFS

Statut : préparation uniquement, `import-only`, `supported=false`. Le domaine
ne doit démarrer qu’après fermeture et handoff explicite de la recompilation
démo. Il ne porte aucune scène, simulation, PPM, Vulkan, FSM ou sémantique
issue du C++ généré.

Profil source inchangé :
[`demo-native-identity-profile-v1.json`](demo-native-identity-profile-v1.json).

## Corpus qualifié

`target_id=ac6-demo-xbox360-pal`, plateforme `xbox360-xenon`, région `PAL-demo`.
Le XEX `Default.xex` est de SHA-256
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` et de
1 454 080 octets. Le corpus exact est :

| fichier | octets |
|---|---:|
| `Default.xex` | 1 454 080 |
| `DATA.TBL` | 13 784 |
| `DATA00.PAC` | 177 340 416 |
| `DATA01.PAC` | 15 138 816 |
| `bgmpack.bin` | 66 455 552 |
| `demopack_eng.bin` | 11 427 840 |
| `demopack_jpn.bin` | 11 675 648 |
| `voicepack_eng.bin` | 16 988 160 |
| `voicepack_jpn.bin` | 21 876 736 |

Total : **322 371 032 octets**. Le store publié doit être séparé de
`ac6-native` et `ac6-demo-recomp`, avec marqueur et chemin XDG propres; aucun
manifest externe ne doit devenir un fichier VFS guest.

## Architecture future minimale

- `reconstruction/ac6-demo-native/config/demo-native-identity-v1.json` : profil
  canonique import-only, sans `.pdata`, Ghidra ou codegen.
- `include/ac6demo_native/content_store.hpp`, `src/content_store.cpp` et
  `src/sha256.cpp` : vérification taille/SHA, source exacte, staging atomique,
  publication sans génération partielle.
- `include/ac6demo_native/vfs.hpp`, `src/vfs.cpp` : namespace read-only
  `game:/` et neuf noms exacts, sans chemin hôte ni échappement.
- `tests/content_store_tests.cpp` et `tests/vfs_boundary_tests.cpp` : corpus
  positif et rejets ci-dessous.
- CMake autonome et futur `ac6-demo-native` limité à `import`/`verify`.
  Aucun lien vers `ac6_product_core`, `ac6_demo_runtime` ou les sorties
  XenonRecomp; aucun `play`/`replay` tant que les traces ne ferment pas les
  frontières runtime.

## Rejets obligatoires

- XEX retail `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`;
- projet Ghidra historique `ace-combat-6-corrected` ou provenance générée;
- fichier tronqué, manquant, taille/SHA incorrecte, marqueur incorrect ou
  mélange retail/démo;
- fichier inattendu (`moviepack.bin`, `default.xex`, temporaire) dans source ou
  store;
- chemins `file:/`, `host:/`, absolus, `game:/../...`, mauvais casse, nom
  inconnu et symlink sortant du store.

Tout rejet doit laisser le store précédent intact et ne publier aucun staging.

## Ce qui est disponible maintenant

Identités des neuf fichiers, taille totale, `target_id`, plateforme/région,
namespace VFS, liste d’exclusions et séparation stricte avec l’identité retail.
Le C++ recompilé ne peut servir qu’au contrôle ABI/flux, jamais à dériver une
sémantique native.

## Ce qui dépend de futures traces

Accès guest post-reprise kernel/XAM, transition persistante après START,
readback non noir, frontend, mission, terminal, observation FSM, replay,
transport MCP et tout statut de support. Tant que ces preuves manquent,
`ac6-demo-native` reste un importateur vérifiable, non un runtime supporté.

Ne pas modifier l’identité, les claims ou le CMake retail; ne pas modifier la
recompilation démo, ses sorties générées, `CURRENT.json` ou les rapports pour
ouvrir ce domaine. Le démarrage est conditionné à la fermeture recomp et à un
handoff qualifié séparé.

# AC6_recomp moderne : prototype poll-exact raw v4

Date : 2026-08-13

## Résultat

Le prototype révision-pinné a passé un **réaudit indépendant sur checkout
propre**. Il reste non intégré à une stack oracle. Le patch
`analysis/oracle/ac6-recomp-ab90b-us/patches/poll-exact-xam-controller-replay-v4.patch`
produit et relit le schéma brut `ac6.controller-input-replay.v4` au seam
manuscrit `XamInputGetState_entry` de ReXGlue.

Il cible exclusivement :

- `AC6_recomp` `ab90b54713e5889f33eee1cc8681dae89fe83d1e`, arbre
  `1e60427e316a2667d189eb1e067a8ec7d776fd50` ;
- ReXGlue embarqué, arbre
  `73589e54e95291a7039de6beada6390ac7c12f78` ;
- XEX NTSC-U/J SHA-256
  `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`,
  title/media `4E4D07D1`/`531C30BE`, version `v0.0.0.8` ;
- manifeste d'identité
  `analysis/oracle/ac6-recomp-ab90b-us/identity.json`.

Le patch contient dix-sept cibles manuscrites. Il ne modifie ni n'embarque
aucun fichier `generated/*`, `ppc_recomp.*` ou `ac6recomp_recomp.*`.

## Frontières implémentées

### Poll XAM

`thirdparty/rexglue-sdk/src/kernel/xam/xam_input.cpp` appelle le service à
l'entrée de `XamInputGetState_entry`, avant toute lecture d'un périphérique
hôte. L'enregistrement conserve :

- ordinal de poll, marqueur et poll dans le marqueur ;
- LR, user, flags et nullité du pointeur comme gardes portables ;
- thread et adresse du pointeur comme diagnostics locaux ;
- résultat XAM et état XInput brut, sans deadzone ni mapping natif ;
- tick invité et index de présentation comme télémétrie seulement.

Les seuls LR admis pour le XEX qualifié sont `0x8234CEE0` et `0x8234CFA4`.
La relecture refuse au premier écart le type d'événement, l'ordinal, le LR,
le user, les flags, la nullité, la forme d'état, les bornes, la troncature,
les événements restants ou le SHA-256 de payload. L'appel physique est
sérialisé avec l'admission du poll ; une exception lève l'admission et marque
le service en échec.

### Marqueur fort

Un symbole manuscrit fort `rex_sub_821CA940` appelle
`ControllerInputMarkerBefore()` avant `__imp__rex_sub_821CA940`. Il ne remplace
pas le corps recompilé : il délègue ensuite à son implémentation faible
générée. Le contrat scellé dans le raw est :

- rôle `ac6_frame_input_stage`, phase `before_input` ;
- adresse `0x821CA940`, RVA `0x001CA940`, longueur 328 ;
- SHA-256 code
  `a4c027fcc05b34b0bb5ad5c8ad6a7f6bd37e2230797549637ee1950338ea390d`.

### Lifecycle

L'initialisation se fait dans `OnPostSetup`, donc après `LoadXexImage` et
avant `LaunchModule`. Elle vérifie au minimum title ID et entry point
`0x821F5ED0`, charge un header canonique externe, puis arme exactement un mode
record ou replay.

ReXGlue reçoit un hook `OnPostGuestShutdown` appelé après le `join` du thread
principal et après la terminaison puis le `join` de tous les XThreads invités
enfant encore actifs. La finalisation y impose l'EOF exact ou auto-valide le
raw avant une publication sans écrasement.

La lecture bornée emploie désormais `open/fstat/read` sur un descripteur
unique. L'initialisation valide un service local puis publie son état en une
seule transaction sous mutex. La sortie est scellée en mémoire, écrite dans un
temporaire exclusif, `fsync`-ée, publiée par hard-link exclusif puis suivie de
deux `fsync` du répertoire. Un échec conserve le payload scellé pour un retry ;
un succès seul réinitialise le lifecycle.

L'impact SDK générique est borné : un hook lifecycle virtuel par défaut vide,
un getter d'horloge non avançant et l'instrumentation du seul service XAM.
Les états et le protocole restent dans les fichiers AC6. Aucun autre service
XAM, driver d'entrée ou chemin d'exécution invité n'est modifié.

## Contrat raw v4 et census v2

Le header brut sépare désormais l'identité oracle NTSC-U/J de son contrat de
marqueur. Une capture complète sort toujours avec :

```text
status=unqualified
integrity_level=null
source_hz=null
native_hz=null
resampling=refuse
census=null
native_clock=null
```

Le runtime ne fabrique donc ni cadence, ni projection, ni attestation. Le
sidecar `ac6.controller-cadence-census.v2` appartient à un runner externe doté
d'une horloge fixe qualifiée. Sa forme reprend le parent raw v4 exact, le
contrat de marqueur, les couples marqueur/séquence et un `reference_tick`
capturé avant chaque marqueur.

Le test croisé a pris le raw C++ à deux marqueurs, construit un census v2
externe à 60 Hz, validé sa filiation, puis produit une fenêtre v4 dérivée par
le lecteur Python. Cela prouve la compatibilité du chemin de qualification,
pas une mesure de cadence du jeu. Le runner doit encore lier une horloge
indépendante au marqueur runtime, par instrumentation bornée ou observateur
manuscrit, puis produire le vrai sidecar.

## Validations

- `git apply --check` puis application sur un worktree détaché propre au commit
  exact : réussi ;
- SHA-256 du patch :
  `d2695e194ae470797a5baec989c651114167cfb3e568397ea901b85fb6c6f85b`,
  81 442 octets ;
- service compilé avec Clang 21, C++23, `-Wall -Wextra -Wpedantic -Werror` :
  réussi ;
- tests record/replay, garde LR, divergence structurée, corruption,
  troncature, profondeur JSON, marqueur vide et EOF : réussis ;
- tests lifecycle répété, 64 polls sérialisés sur quatre threads, lecture
  bornée mono-descripteur et échec/collision puis retry de publication :
  réussis ;
- configuration et CTest avec `AC6_ORACLE_BUILD_TESTS=ON` et
  `BUILD_TESTING=OFF` : réussis ;
- raw déterministe de test accepté par le lecteur Python v4 gelé, SHA-256
  `80c09e029c3d920b6567f49737bbe3ecce6039410803c0cd5cead2c4f6821733` ;
- construction/validation d'un census externe v2 puis reseal v4 : réussie ;
- syntaxe ciblée du runtime, de XAM, du lifecycle, de l'horloge et de l'app :
  réussie contre les headers codegen déjà qualifiés ;
- garde de frontières : dix-sept chemins manuscrits, zéro cible generated ou
  PPC recompilé, wrapper BEFORE et finalisation après join contrôlés ;
- réaudit depuis un second clone détaché propre de `ab90b547…` : application,
  compilation Clang 21 `-Werror`, tests, hash fixture et garde de frontières
  réussis ;
- `git diff --check` : réussi ; aucun lancement interactif ni byte retail.

## État et risques résiduels

Le résultat reste `provisional-rexglue` : zéro lane et zéro gate M01 fermé.

- le runtime Linux moderne complet reste bloqué en amont par les includes et
  chemins D3D12/Windows consignés au cycle 1570 ; seule la syntaxe ciblée est
  validée ici ;
- le header fournit les SHA-256 du binaire et du build depuis le harness, mais
  le runtime ne les recalcule pas : `source_lineage_verified=false` ;
- title ID et entry point sont vérifiés en mémoire, pas le SHA-256 intégral du
  XEX chargé ;
- aucun census runtime, aucune cadence 30/60, aucun replay boot→M01 et aucune
  comparaison gameplay n'ont été exécutés ;
- la durabilité Windows repose sur `FlushFileBuffers` avant publication ; la
  double synchronisation de répertoire n'est disponible que sur POSIX ;
- `present_index` et `guest_tick` restent des diagnostics et ne doivent jamais
  être promus en horloge de qualification.

Ce checkpoint autorise maintenant l'intégration contrôlée et la préparation du
runner de census. Toute utilisation de gate reste refusée jusqu'au build
runtime, à l'attestation de lignée et à la mesure externe de cadence.

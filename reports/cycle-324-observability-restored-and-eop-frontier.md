# Cycle 324 — l'observabilité était coupée par défaut ; le front devient la source 1

## Cible

- Produit : AC6 Xbox 360 PAL, Xenon PPC big-endian, Xenos
- Module : `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Base image : `0x82000000`
- Exécutable natif : `ad976caddc3b5600b566a02ef405f8c44f5e9c0e675da16d2a3fc07789edd055`
- Route : `dynamic`

## 0. Ce que ce cycle change

Le cycle 323 avait laissé comme première action « remonter l'observabilité », en
accusant trois correctifs d'instrumentation archivés et non appliqués. **Ce
n'était pas la cause.** La cause est un défaut par défaut du produit :

```text
src/main.cpp:60   REXCVAR_DEFINE_BOOL(ac6_performance_mode, true, ...)
src/main.cpp:160  ApplyAc6PerformanceModeOverrides(): REXCVAR_SET(log_level, "error")
```

`ac6_performance_mode` vaut **`true` par défaut**, et il force
`log_level = "error"`. Le runtime livre donc ses propres diagnostics muets. Une
exécution nue émet exactement **trois** lignes `[info]` depuis
`Ac6recompAppCreate` puis plus rien de sa vie, et `ac6recomp.log` reste à
**0 octet**.

Mesure du contraste, même binaire, même durée :

| | défaut | `--ac6_performance_mode=false` |
|---|---:|---:|
| lignes dans `ac6recomp.log` | **0** | **716** |
| lignes sur la sortie standard | 4 | 4 |

Toute mesure « le runtime ne fait rien » prise depuis le cycle 318 a donc été
prise à l'aveugle. C'est aussi ce qui explique que les cycles 318 à 322 se soient
rabattus sur `gdb` : le journal ne servait plus à rien, sans qu'aucun fichier ne
le dise. Un commentaire isolé dans `src/ac6_pac_decoder_probe.cpp:37` avait déjà
relevé le piège ; il n'était ni dans le plan, ni dans l'outillage.

## 1. Télémétrie posée dans l'arbre

Plutôt que réappliquer les trois correctifs archivés — dont la disparition était
précisément le défaut relevé au cycle 323 — les compteurs vivent maintenant dans
l'arbre, dans `rex::graphics::frame_loop_telemetry`, et survivent aux
régénérations. Ils sont toujours maintenus ; seule l'émission est gardée, par le
cvar `frame_loop_telemetry_interval` (`0` = silencieux, défaut).

Trois étages du pipeline de présentation sont comptés séparément, parce que les
confondre est exactement l'erreur qui a été commise :

| compteur | site | sens |
|---|---|---|
| `guest_swap_requests` | `VdSwap_entry`, avant toute validation | l'invité **demande** une image |
| `host_swap_presents` | site du `XELOG_GPU PRESENT` réussi | l'hôte **présente** l'image |
| `pacing_notifications` | `NotifyGuestPresent` | étage plus tardif, atteint seulement si le cadencement est armé |

Le premier jet de ce cycle comptait l'hôte à `NotifyGuestPresent` et rapportait
`host_present_deliveries=0` alors que le journal contenait trois
`XELOG_GPU PRESENT`. Le compteur était au mauvais étage, pas l'hôte en faute :
`NotifyGuestPresent` n'est pas sur ce chemin. Corrigé en comptant à l'étage que
le cycle 316 avait apparié à `VdSwap`, sans quoi les deux cycles ne sont pas
comparables.

Artefact : `patches/rexglue-in-tree-frame-loop-telemetry-20260730.patch`.

## 2. Mesure

`tools/ac6-frame-loop-probe.sh cycle324 90`, 90 s, 18 lignes de télémétrie.

```text
premier échantillon (vblank=300)  : eop=12 no_handler=19
                                    guest_swap_requests=4 host_swap_presents=3
                                    pacing_notifications=0
dernier échantillon (vblank=5400) : eop=12 no_handler=19
                                    guest_swap_requests=4 host_swap_presents=3
                                    pacing_notifications=0
```

Chronologie complète du journal invité, 726 lignes :

| évènement | fenêtre |
|---|---|
| 129 lectures `DATA00.PAC` | 13:15:30.411 → **13:15:31.513**, soit 1,1 s |
| `XELOG_GPU PRESENT` ×3 | 30.138, 31.281, 31.446 |
| `Skipping Vulkan frame presentation due to async placeholder draw usage in this frame` ×1 | 31.296 |
| interruptions vblank (source 0) | **5 400**, régulières, ~60 Hz, sur les 90 s |
| interruptions EOP (source 1) | **12**, toutes dans la fenêtre initiale |
| lignes de journal après 13:15:32, hors audio et hors télémétrie | **0** sur 88 s |

L'invité vit donc **1,5 seconde**, puis se tait pendant 88 secondes en recevant
5 400 interruptions vblank.

## 3. Deux faits nouveaux

### 3.1 L'hôte n'honore pas exactement ce que l'invité demande

Le cycle 316 §2 concluait : « Les deux nombres sont **égaux**. Le chemin de
présentation hôte honore donc exactement ce que l'invité demande. **Il n'y a
aucun défaut côté hôte.** » C'était vrai de son échantillon `2/2`.

Mesuré ici : **4 demandes, 3 présentations, 1 refus explicite**, avec une cause
nommée — usage d'un dessin de remplacement asynchrone dans cette trame. Le refus
tombe entre la deuxième et la troisième présentation, pas en dernier, donc il
n'est pas trivialement « la dernière demande perdue ». La conclusion du cycle 316
doit être requalifiée : le chemin hôte honore 3 demandes sur 4 et en décline une
pour un motif qu'il journalise.

Confiance : `confirmed` pour les comptes et l'existence du refus ; **aucune**
attribution causale n'est faite ici entre ce refus et l'arrêt de l'invité.

### 3.2 La source 1 est morte, la source 0 tourne toujours

C'est le front le plus net disponible.

Le cycle 317 §1 avait établi que la branche **travail** du gestionnaire invité
`sub_821E63B0` est réservée à la **source 1** (`cmplwi cr6,r3,1`), et que la
source 0 emprunte l'autre chemin. Le cycle 317 §5 avait ensuite montré que le
chemin source 0 appelle bien `sub_821EFBA0` à chaque interruption.

Donc : la branche qui compte s'est exécutée **12 fois puis jamais plus**, alors
que la source 0 continue 5 400 fois. Les interruptions EOP sont produites quand
le flux de commandes GPU atteint un évènement de fin de pipe, c'est-à-dire quand
l'invité soumet du travail. L'état observé est une boucle refermée sur elle-même :
l'invité ne soumet plus, donc pas d'EOP, donc la branche travail ne tourne pas,
donc l'invité ne soumet pas.

La question du cycle suivant n'est donc plus « pourquoi `sub_821EFBA0` ne dessine
pas » — cette fonction est sur le chemin source 0, qui n'est pas la branche
travail — mais : **qu'est-ce qui a produit les 12 premières EOP, et pourquoi la
13e n'arrive-t-elle pas ?**

## 4. Nettoyage et outillage

- `tools/ac6-advance-loop.sh` : `timeout N xvfb-run … ac6recomp` ne bornait pas
  l'invité, il signalait le script `xvfb-run`, et le petit-fils survivait. C'est
  le mécanisme derrière le `gdb` de trois jours et les 28 réservations de mémoire
  invité orphelines du cycle 323. Remplacé par un `bounded_run` qui tue le groupe
  de processus, récolte l'Xvfb orphelin et appelle la garde de fuite.
  `CLAUDE_JOB_DIR` a aussi une valeur de repli, il était utilisé sans garde.
- `tools/ac6-frame-loop-probe.sh` : passe désormais
  `--ac6_performance_mode=false` (obligatoire, §0), `--log_flush_interval=1`
  (le sink fichier n'est vidé qu'à l'arrêt et chaque sonde finit en SIGKILL) et
  `--frame_loop_telemetry_interval`. Il récolte l'Xvfb, nettoie les fuites, et
  distingue explicitement « aucune ligne de télémétrie » de « zéro image » —
  parce que confondre les deux est le piège que le cycle 323 avait signalé.
- `exports-invalid-raw-pointer/`, répertoire vide de sortie connue mauvaise :
  supprimé.

## 5. Validation exécutée

```text
build avec télémétrie, 3 fois (compteur, étage corrigé, final)   rc=0, 0 warning nouveau
binaire final                            ad976cad…dd055
sonde 90 s                               18 lignes de télémétrie, 0 REX_FATAL
contraste ac6_performance_mode           0 ligne vs 716 lignes de journal
étage hôte recompté                      host_swap_presents=3 == XELOG_GPU PRESENT ×3
valeur invité 0x82870828, 8 échantillons 0, stable (inchangé depuis le cycle 323)
bash -n sur les deux outils modifiés     PASS
garde de fuite, exécution réelle         431 Mio récupérés
```

Ni preuve de jouabilité, ni preuve de parité retail.
`recompiler-generated` n'est pas `verified`.

## 6. Front suivant

1. **Pourquoi la 13e interruption EOP n'arrive-t-elle pas ?** Identifier ce qui
   produit une EOP côté hôte, et ce que l'invité a soumis 12 fois. C'est la
   branche travail du gestionnaire `sub_821E63B0`, donc le seul chemin qui peut
   dessiner.
2. Qualifier le refus « async placeholder draw usage » : une trame refusée
   sur quatre est-elle une conséquence de l'arrêt de l'invité, ou une cause ?
   Ne pas conclure sans un contraste.
3. Reprendre la lecture de ce que l'invité fait dans sa fenêtre de 1,5 s : 129
   lectures `DATA00.PAC` puis silence, alors que le cycle 314 en comptait 47 en
   0,4 s. Le profil de chargement a changé et n'a jamais été réexaminé.
4. Restent ouverts : `REX_FATAL` alertable de `sub_821F5828`, limite
   `WaitMultiple` du cycle 323, réconciliation des deux projets Ghidra.

Règle ajoutée : **vérifier que le canal de mesure fonctionne avant de conclure
d'une absence.** Le cycle 323 a lu `PRESENT 0 / VdSwap 0` et a correctement
refusé d'y voir zéro image — mais il a attribué le silence aux correctifs
manquants alors qu'un défaut par défaut du produit coupait tout le journal.

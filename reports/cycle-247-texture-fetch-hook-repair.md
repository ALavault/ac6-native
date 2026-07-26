# AC6 cycle 247 — correction du hook de fetch texture

## Identités

- target retail : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- checkout AC6Recomp : `.tools/ac6-recomp-reference` ;
- commit de base : `c5b089fb6988ac504ba394db611543bda2fb2c96`.

Le checkout était propre avant cette modification. Aucun fichier généré,
binaire retail, cache ou projet Ghidra n'est modifié.

## Audit de frontière

Le fichier `ac6recomp_config.toml` déclare `0x821E10C8` dans `[functions]` et
non dans `[[midasm_hook]]`. ReXGlue émet pour ces entrées un symbole faible
`rex_sub_*` aliasé vers `__imp__rex_sub_*`; le `PPC_FUNC_IMPL` fort dans
`src/d3d_hooks.cpp` remplace donc le chunk généré puis appelle explicitement
`__imp__rex_sub_821E10C8` pour poursuivre le code retail.

Le projet Ghidra canonique confirme cependant les vraies bornes physiques :

- `0x821E1088` est appelé par six sites et possède le prologue ;
- `0x821E10C8` n'a aucune xref d'appel et se trouve dans ce corps ;
- `0x821E1208` possède le prologue suivant et trois xrefs d'appel ;
- `0x821E1248` n'a aucune xref d'appel et se trouve dans ce second corps.

Le hook `0x821E10C8` est donc un chunk remplaçable volontaire, mais son ancien
commentaire lui attribuait à tort le contrat d'une entrée de fonction. À ce
point, `0x821E10B8` a déjà exécuté `r4 = r31 + r4`, avec `r31 = r3 = device`.

## Correctif minimal

Le hook conserve son adresse, son symbole et son appel à la continuation
générée. Il reconstruit seulement :

```text
stage = uint32(mutated_r4 - device)
```

La soustraction unsigned inverse exactement l'addition PPC 32 bits, y compris
en cas de wraparound. Le helper
`ac6::d3d::RecoverTextureStageAt821E10C8()` vit dans `src/d3d_state.h`, avec
trois assertions de compilation couvrant les stages 0, 31 et le wraparound.
Le hook indexe maintenant `texture_fetch_ptrs[stage]` et journalise `stage`,
sans changer le chemin retail ni activer de rendu.

Fichiers modifiés dans le checkout de référence :

- `.tools/ac6-recomp-reference/src/d3d_state.h` ;
- `.tools/ac6-recomp-reference/src/d3d_hooks.cpp`.

La configuration et les sorties XenonRecomp restent intactes.

## Validation

### Preuve XEX headless

`artifacts/ac6-cycle247-function-boundary-audit.log` conserve les instructions
et xrefs bornées autour de `0x821E1030..0x821E1248`. Le vérificateur du cycle
246 reste la preuve exécutable de l'état des registres à la frontière.

### Contrat isolé

Un probe C++23 compilé avec Clang 21 teste les 32 stages :

- l'ancien contrat utilise `device+stage` et rejette les 32 valeurs comme hors
  de `kMaxFetchConstants` ;
- le nouveau helper restitue exactement chaque stage `0..31` ;
- le cas de wraparound restitue 31.

Résultat : compilation et exécution `PASS`. Artefact :
`artifacts/ac6-cycle247-texture-stage-contract-test.log`.

### Syntaxe du hook réel

`src/d3d_hooks.cpp` passe Clang 21 en `-std=c++23 -fsyntax-only -Wall -Wextra
-Werror` avec les headers ReXGlue vendored. Artefact :
`artifacts/ac6-cycle247-d3d-hooks-syntax.log`.

### Build complet

La configuration Linux complète reste indisponible : le SDK exige le paquet
de développement `gtk+-3.0`, absent de `pkg-config`. Le journal
`artifacts/ac6-cycle247-cmake-configure.log` enregistre ce blocage exact. Les
absences optionnelles de `libunwind` et `libusb-1.0` ne sont pas la cause de
l'arrêt. Aucun paquet n'est installé et aucune session humaine n'est requise
pour poursuivre l'analyse statique.

## Limites et prochaine frontière

Le correctif est validé au niveau source et contrat, mais pas encore dans un
exécutable AC6Recomp Linux complet. Une future installation explicite de
`libgtk-3-dev` permettra la régénération/build sans réouvrir l'analyse du hook.

Le prochain front statique est le slot texture byte `+0x2C` : qualifier les
appels `0x821DC4F8`, `0x821DC688` et `0x821DC908` comme état sampler ou autre
contrat, puis conserver séparément la sélection shader/permutation. Aucun run
Xenia, VNC ou humain n'est demandé.

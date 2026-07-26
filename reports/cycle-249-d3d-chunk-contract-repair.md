# AC6 cycle 249 — correction des chunks sampler, stream et index

> **Correction cycle 251 :** les conclusions sampler de ce rapport restent
> valides, mais les étiquettes stream/index ci-dessous sont obsolètes.
> `0x821DD068` est le vrai setter de streams ; `0x821DD220/0x821DD258`
> capture les quatre render targets couleur ; `0x821DD588/0x821DD5C8`
> capture la surface depth-stencil séparée. Voir
> `cycle-251-resource-binding-semantic-correction.md`. Ne réutiliser ni les
> anciens noms de champs ni les compteurs stream/index de ce rapport.

## Identité et portée

- target : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- projet Ghidra canonique : `ace-combat-6` ;
- checkout AC6Recomp : `.tools/ac6-recomp-reference`, base
  `c5b089fb6988ac504ba394db611543bda2fb2c96`.

L'audit est headless, en lecture seule et borné aux chunks D3D configurés qui
alimentent le shadow state. Aucun XEX, projet Ghidra, fichier généré ou
configuration XenonRecomp n'a été modifié.

## Quatre états sampler rétablis

Les quatre autres adresses sampler configurées sont elles aussi des chunks
internes, et non des entrées de fonctions autonomes :

| Chunk | Fonction physique | Valeur récupérable | Champ Xenos écrit |
| --- | --- | --- | --- |
| `0x821DC9C0` | `0x821DC980` | `r4=sampler`, `r5=float brut` | `aniso_bias`, dword 5 bits 5..8 |
| `0x821DCA68` | `0x821DCA28` | `r4=sampler`, `r5=float brut` | `lod_bias`, dword 4 bits 12..21 |
| `0x821DCB08` | `0x821DCAC8` | `r4=sampler`, `r11=valeur clampée` | `mip_min_level`, dword 4 bits 2..5 |
| `0x821DCB88` | `0x821DCB48` | `r4=sampler`, `r11=valeur clampée` | `mip_max_level`, dword 4 bits 6..9 |

Les anciennes étiquettes `border_color`, `mip_filter`, `mip_level` et
`min_filter` étaient donc fausses pour ces quatre frontières. Le shadow state
conserve maintenant les deux flottants bruts et les deux valeurs clampées. Les
champs `mip_filter` et `border_color` restent explicitement non couverts plutôt
que d'être alimentés par des données sans rapport.

## Vraies frontières stream et index

La fonction `0x821DD220` reçoit `r3=device`, `r4=stream` et `r5=resource`. Elle
stocke le pointeur dans `device+0x3090+4*stream`. Les chemins de reset itèrent
explicitement sur quatre slots, `0..3`. Au chunk configuré `0x821DD258`, les
trois valeurs d'entrée sont encore disponibles avant que l'instruction
remplacée ne réutilise `r3`. Ce chunk alimente désormais `streams[stream]` et
le compteur `set_stream_source_calls`.

La fonction distincte `0x821DD588` reçoit `r3=device`, `r4=resource`, conserve
ces valeurs dans `r31/r30` puis stocke le pointeur à `device+0x30A0`. Les
appelants et chemins de reset traitent cette ressource séparément des quatre
streams. Le chunk configuré `0x821DD5C8` alimente donc le buffer d'index depuis
`r30` et mémorise le device depuis `r31`.

L'ancien hook `0x821DD1C8` ne peut pas capturer le buffer d'index : à cette
frontière, l'argument original a déjà été sauvegardé dans `r29` et `r4` a été
réutilisé. Il reste un simple pass-through jusqu'à qualification indépendante
de sa fonction physique `0x821DD188`.

## Modifications source

Dans le checkout externe uniquement :

- `src/d3d_state.h` expose les champs sampler qualifiés ;
- `src/d3d_hooks.cpp` corrige les quatre chunks sampler, neutralise le faux
  hook d'index et ajoute les captures stream/index aux chunks qualifiés ;
- `src/ac6_backend_fixes/ac6_backend_capture_bridge.cpp` consomme les nouveaux
  champs sans prétendre disposer d'un mip filter ou border color qualifié.

Les continuations générées restent exécutées. Les champs offset/stride du
stream ne sont pas inventés : le corps retail qualifié n'expose ici que le
pointeur de ressource.

## Validation

- `VerifySamplerChunkContracts.java` : **24/24** assertions exactes ;
- `VerifyStreamIndexChunkContracts.java` : **19/19** assertions exactes ;
- huit xrefs directs vers `0x821DD220` et neuf vers `0x821DD588` ont été
  inspectés ;
- `d3d_hooks.cpp` passe Clang 21 avec C++23, `-Wall -Wextra -Werror` et
  `-fsyntax-only` ;
- le bridge de capture et les hooks passent ensemble le probe syntaxique
  précédent après la mise à jour des champs.

Artefacts principaux :

- `artifacts/ac6-cycle249-sampler-chunk-audit.log` ;
- `artifacts/ac6-cycle249-sampler-chunk-validation.log` ;
- `artifacts/ac6-cycle249-stream-source-candidates.log` ;
- `artifacts/ac6-cycle249-stream-source-xrefs.log` ;
- `artifacts/ac6-cycle249-stream-source-callers.log` ;
- `artifacts/ac6-cycle249-stream-index-validation.log` ;
- `artifacts/ac6-cycle249-d3d-hooks-syntax.log`.

## Limites et prochaine frontière

Cette passe fiabilise la capture des ressources et états concernés ; elle ne
prouve pas une équivalence renderer. La fonction physique `0x821DD188`, les
chunks D3D restants et la jointure MATE vers technique/passe/permutation puis
draw demeurent ouverts. Aucun run Xenia, VNC, GUI ou geste humain n'est requis
pour leur prochaine inspection statique.

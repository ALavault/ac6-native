# AC6 retail NTSC-U/J — r458 — le rejet MSAA de r457 est un VRAI trou de couverture (cible EDRAM réellement en 4× MSAA), pas un bug de décodage — corrige la prévision de r457, aucun correctif rapide possible ce cycle

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r457 : investiguer le motif de rejet « MSAA render targets
are not qualified this cycle » (`derive_edram_render_target()`,
`native/src/native_vulkan_backend.cpp`), avec l'hypothèse (posée par
r457, non vérifiée) qu'il s'agirait probablement d'un autre correctif
ciblé sans besoin d'oracle, sur le modèle du bug de garde corrigé en
r457.

## Établi — un vrai diagnostic en direct, pas une supposition

`derive_edram_render_target()` lit `RB_SURFACE_INFO` (registre Xenos
`0x2000`) et rejette si les bits 16:17 (nombre d'échantillons MSAA)
sont non nuls. Pour vérifier si cette valeur reflète un état MSAA
réel ou un décodage erroné, une trace de diagnostic locale (gardée
par `AC6_NATIVE_VD_TRACE`, non committée, même statut que les ajouts
de r455) a été ajoutée juste avant le rejet, imprimant la valeur brute
du registre.

Relancé contre l'ISO réelle (`AC6_NATIVE_VD_TRACE=1
AC6_NATIVE_CAPTURE=1`, fenêtre 20 s) :
```
r458 probe: RB_SURFACE_INFO=0x0a020280 msaa_bits=2 pitch_pixels=640
```
`msaa_bits=2` correspond, dans l'encodage Xenos standard
(`MSAA_NumSamples` : 0=1×, 1=2×, 2=4×), à **une véritable cible de
rendu EDRAM en 4× MSAA**. `pitch_pixels=640` est cohérent avec une
résolution effective liée (moitié de 1280, plausible pour un tuilage
lié au nombre d'échantillons — non creusé plus loin ce cycle). **Ce
n'est pas une valeur de registre corrompue ou mal décodée** — le jeu
demande réellement une cible multi-échantillonnée à cet instant.

## Ce que ceci corrige

**La prévision de r457 était fausse** : ce motif n'est PAS, comme
supposé, « probablement un autre correctif ciblé sans besoin
d'oracle, même modèle que r457 ». r457 avait trouvé un vrai bug de
garde (une condition mal placée, sans rapport avec l'état matériel
réel) ; ce cycle établit que le rejet MSAA correspond, lui, à un
**vrai trou de couverture fonctionnelle** : `PinnedShaderRuntime` ne
sait tout simplement pas encore créer/résoudre une cible EDRAM
multi-échantillonnée (image Vulkan multisample + passe de résolution
vers la cible non multi-échantillonnée) — une fonctionnalité réelle
et substantielle à construire, pas un bug d'une ligne.

## Non établi

- **L'étendue exacte du travail requis** pour qualifier le rendu 4×
  MSAA (création d'image Vulkan `VK_SAMPLE_COUNT_4_BIT`, `vkCmdResolveImage`
  ou attachement de résolution dans le sous-passe, compatibilité avec
  le format/pitch déjà géré pour le cas 1×) — non dimensionné ce
  cycle.
- **Si `pitch_pixels=640` reflète une convention d'encodage liée au
  nombre d'échantillons** (à diviser/multiplier selon le nombre
  d'échantillons) qui affecterait aussi le code de pitch existant une
  fois le support MSAA ajouté — non investigué.

## Décisions prises

- Ne pas tenter d'implémenter le support MSAA ce cycle — c'est un
  vrai morceau de travail de moteur de rendu (pas une correction de
  bug), mérite son propre cycle dédié plutôt qu'être bâclé en fin de
  cycle d'investigation.
- Garder la trace de diagnostic ajoutée (gardée par
  `AC6_NATIVE_VD_TRACE`, non committée) — cohérent avec le motif de
  diagnostic établi en r455, utile pour un futur cycle qui
  implémenterait le support MSAA.
- Toujours ne pas committer `native_vulkan_backend.cpp` — même
  entremêlement avec l'arriéré que tous les cycles r454-r457.

## Gate

**Aucune source committée ce cycle** — la trace de diagnostic reste
appliquée localement, vérifiée (build + `ctest` 11/11 + lancement réel
avec trace), non committée.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées — aucune source de production committée ce cycle)
`ctest` racine inchangé (aucune source de production committée ce
cycle).

## Named for r459

Deux pistes distinctes, ni l'une ni l'autre bloquante : (1) dimensionner
et implémenter le support d'une cible EDRAM 4× MSAA dans
`PinnedShaderRuntime` (image Vulkan multi-échantillonnée + résolution) —
un vrai morceau de travail de moteur, probablement son propre cycle
dédié, voire plusieurs ; (2) le premier motif de rejet caractérisé par
r456 (couverture du registre épinglé, 271 variantes) nécessite
probablement une session oracle — décision utilisateur, pas à prendre
seul. Reste ouvert sinon : une fois le câblage jugé mûr, reconsidérer
le committage groupé de l'arriéré (r433/r434/r438/r454-r458) ; mise à
jour de `tools/prepare.py`/`tools/build.py` pour le chemin ISO par
défaut (confort, pas une nécessité).

## Files

Aucun artefact gitignoré nouveau (journal de vérification sous
`/fastdata/tmp/...scratchpad/`, non conservé).

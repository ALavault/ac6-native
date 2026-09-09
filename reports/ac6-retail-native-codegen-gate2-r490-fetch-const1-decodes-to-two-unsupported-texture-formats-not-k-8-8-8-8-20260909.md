# AC6 retail NTSC-U/J — r490 — décodage de `fetch_const[1]` (r475) : les deux valeurs observées (`0x10000056`, `0x1000001a`) codent des formats de texture NON supportés par `decode_pixel_texture()` (`k_24_8`, `k_16_16_16_16` — ni l'un ni l'autre n'est `k_8_8_8_8`), pas une confirmation directe de ce que les 86 tirages échantillonnent

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé, aucun lancement (sondage natif ou route oracle)
ce cycle — analyse statique uniquement, sur les valeurs déjà rapportées
par r475 et sur le code déjà présent dans ce dépôt.

## Contexte

Nommé par r475 (« Non établi » et « Named for r476 ») : le rôle exact
de `fetch_const[1]=0x10000056`/`0x1000001a`, un motif répété observé
pendant le sondage natif `--probe-entry`, jamais décodé. r488/r489
ayant fermé la piste « movie worker » et caractérisé la contention
hôte comme cause de la volatilité de capture, l'utilisateur a choisi
de poursuivre sur une piste d'analyse statique indépendante plutôt que
de relancer une capture oracle sur une machine encore chargée.

## Établi — identité exacte du registre : dword 1 de la constante de texture fetch, unité de texture 0 (t0)

`recompilation/ace-combat-6-retail/native/src/native_guest_vd.cpp:394-403` :
la trace `vd fetch_const[%u]=0x%08x` parcourt linéairement les 192
DWORDs du bloc de constantes de texture fetch (`kRegShaderConstantFetch000
= 0x4800`, `kFetchConstantDwords = 192`), et n'affiche que les DWORDs
non nuls — `dword` dans le format est donc directement l'offset brut
dans ce bloc de 192 mots, PAS un compteur des occurrences non nulles.
Puisque chaque constante de texture fetch occupe 6 mots consécutifs
(confirmé par `native_vulkan_backend.cpp:1085-1088` :
`words[i] = state.register_value(kRegShaderConstantFetch000 + 6u *
fetch_constant + i)`), l'offset 1 correspond exactement à
`fetch_constant=0, i=1` — c'est-à-dire le **second mot (dword_1)** de
la constante de texture fetch pour **l'unité de texture t0**, pas un
« deuxième » fetch constant.

## Établi — la disposition de bits que ce dépôt utilise déjà est confirmée correcte contre la source Xenia publique

`native_vulkan_backend.cpp:1121-1123` documente et implémente déjà :
```
format 0:5, endianness 6:7, request_size 8:9, stacked 10,
nearest_clamp_policy 11, base_address 12:31 (page >> 12)
```
Comparé directement (lecture GitHub, pas un run d'oracle — même
méthodologie que r462/r487/r488) à `src/xenia/gpu/xenos.h` du dépôt
public `xenia-project/xenia` (branche `master`), struct
`xe_gpu_texture_fetch_t`, champs de `dword_1` (lignes ~1221-1227) :
```
TextureFormat format : 6;           // +0 dword_1
Endian endianness : 2;              // +6
uint32_t request_size : 2;          // +8
uint32_t stacked : 1;               // +10
uint32_t nearest_clamp_policy : 1;  // +11
uint32_t base_address : 20;         // +12 base address >> 12
```
**Disposition identique bit pour bit.** Le décodeur de ce dépôt n'a
donc PAS de bug de disposition de bits sur ce mot — vérifié, pas
supposé.

## Établi — décodage exact des deux valeurs observées par r475

```
0x10000056: format=22 (0x16) endian=1 request_size=0 stacked=0 nearest_clamp=0 base_address=0x10000000
0x1000001a: format=26 (0x1a) endian=0 request_size=0 stacked=0 nearest_clamp=0 base_address=0x10000000
```
`TextureFormat` (même source Xenia, `xenos.h` lignes ~450-497) :
- **22 = `k_24_8`** — un format profondeur/stencil (24 bits profondeur
  + 8 bits stencil), PAS un format couleur/texture d'interface typique.
- **26 = `k_16_16_16_16`** — un format couleur 4 canaux 16 bits fixes.
- `Endian` (même source, lignes 193-198) : 1 = `k8in16` (octets permutés
  par paires de 16 bits) ; 0 = `kNone`.

**Aucune des deux valeurs ne décode `format=6` (`k_8_8_8_8`)** — le
SEUL format que `decode_pixel_texture()`
(`native_vulkan_backend.cpp:1124-1129`) accepte actuellement :
```cpp
if (format != 6u) {  // TextureFormat::k_8_8_8_8
  error_ = "pinned texture: texture format is not k_8_8_8_8 ...";
  return false;
}
```
**Si ces registres sont bien ceux que les tirages rejetés
lisent réellement**, ils échoueraient à cette vérification même si
le trou de couverture du registre de nuanceurs épinglés (r456/r472)
était comblé — un second motif de rejet potentiel, indépendant du
premier.

## Non établi — la corrélation directe entre ces valeurs et les 86 tirages rejetés

Le journal brut du sondage qui a produit ces deux valeurs (r475) n'a
pas été conservé (scratch, `/fastdata/lavaulta/tmp/`), et ce cycle n'a
délibérément relancé AUCUN sondage (contention hôte documentée par
r489, et périmètre de ce cycle limité à l'analyse statique). Il n'est
donc PAS établi que ces deux valeurs proviennent spécifiquement des
tirages `vertex_count=1 index_count=1` (les 86 tirages d'interface
identifiés par r475) plutôt que d'un contenu de registre résiduel
laissé par un tirage antérieur non lié — le bloc de trace se déclenche
sur `draw != 0` pour tout le lot, sans corrélation per-tirage vers
quel shader lit réellement le fetch constant 0. `k_24_8` est en
particulier un format profondeur/stencil inhabituel pour un élément
d'interface texturé ; une hypothèse plausible (échantillonnage d'un
depth-buffer résolu pour un effet de composition) reste spéculative,
non vérifiée.

- **Si ces deux valeurs représentent réellement DEUX textures
  différentes liées à des moments différents**, ou une seule valeur
  résiduelle jamais réellement lue par le nuanceur actif — non
  distingué sans une trace corrélée par tirage (hors périmètre borné
  de ce cycle).
- **Si `decode_pixel_texture()` devrait être étendu pour supporter
  `k_24_8` et/ou `k_16_16_16_16`** — dépend de la réponse à la question
  précédente ; pas de correctif tenté ce cycle (aucune confiance
  suffisante sans corrélation per-tirage établie, conformément à la
  discipline de preuve de ce dépôt).

## Décisions prises

- Ne PAS modifier `native/` ce cycle — le lien entre ces deux valeurs
  et les tirages rejetés spécifiques n'est pas établi ; ajouter un
  support de format sur cette seule base serait une supposition, pas
  une dérivation.
- Ne PAS relancer de sondage natif ce cycle — la contention hôte
  documentée par r489 (charge système ~40-52 encore mesurée juste
  avant ce cycle) rendrait tout nouveau résultat aussi peu fiable que
  les tentatives de capture oracle des cycles précédents, et le
  périmètre de ce cycle (décodage statique) ne le nécessitait pas.
- Citer directement `xenia-project/xenia` (lecture GitHub, pas un run
  d'oracle N3) pour la disposition de bits et les noms de format,
  cohérent avec la méthodologie déjà établie par r399/r462/r487/r488.

## Gate

Aucune source de production modifiée ce cycle (analyse uniquement).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
`ctest` racine et natif inchangés (aucune source de production
modifiée ce cycle).

## Named for r491

**Pour établir la corrélation per-tirage** (ce qui manque pour décider
d'un éventuel support de format), il faudrait soit (1) relancer le
sondage natif avec une trace qui associe explicitement chaque valeur
de fetch constant au tirage qui l'utilise réellement (un
`AC6_NATIVE_VD_TRACE` étendu, modification de code triviale mais
nécessitant un nouveau lancement — à faire une fois la contention hôte
retombée), ou (2) lire directement le nuanceur pixel des tirages
rejetés via Ghidra pour voir s'il référence effectivement l'unité de
texture t0 (méthode statique, même approche que r487/r488, pas besoin
d'attendre la contention). Reste ouvert sinon : décision de committage
groupé de l'arriéré natif restant (si applicable après r481-r489) ;
piste 2 de r488 (chercher un autre mécanisme d'attente bloquant via
lecture Ghidra directe sur un point de blocage observé) toujours non
tentée.

## Files

Aucun artefact gitignoré nouveau conservé (`xenos.h` téléchargé
temporairement sous `/fastdata/lavaulta/tmp/r490-xenos.h` pour
lecture, non conservé).

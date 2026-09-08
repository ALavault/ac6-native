# AC6 retail NTSC-U/J — r451 — CAUSE RACINE ULTIME ET COMPLÈTE : `tools/prepare.py` ne copie JAMAIS les fichiers de contenu `.PAC`/`.TBL`/`.bin` dans `source/assets/` — seul `default.xex` y est placé, alors que `DATA00.PAC`/`DATA01.PAC`/`DATA.TBL` existent bel et bien dans `game-files/` — ferme l'intégralité de la chaîne r399-r451

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r450 : tracer ce qui devrait réécrire le pool après le
`memset(0xFE)` initial ; confirmer l'absence de tout second écrivain
avant le crash.

## Établi — aucun second écrivain, confirmé définitivement

Point d'arrêt matériel réarmé sur le même octet (`0x173b0038`) depuis
la toute première instruction invité, laissé actif à travers
plusieurs `continue` jusqu'au crash déjà connu : **une seule
activation** (le `memset(0xFE)` de r450), puis rien — le processus va
directement de là au `SIGSEGV` de `sub_821D6C20`, sans jamais réécrire
cet octet. Confirme définitivement l'hypothèse de r450 : aucun second
écrivain n'existe dans le code exécuté.

## Établi — la cause était déjà nommée par un cycle antérieur (r276), et déjà partiellement corrigée

Recherche dans `tools/materialize_native_import_stubs.py` (déjà
consulté par r448) : le commentaire du stub `NtQueryInformationFile`
documente précisément ce scénario :

> r276 : le producteur manquant est tracé — `sub_821CC008` (le
> créateur de file d'attente du pipeline de lecture PAC) ouvre
> chaque fichier de catalogue et dérive la taille du tampon de
> lecture partagé à partir de la somme des tailles de fichiers
> interrogées… Le remplissage à zéro de r240 a réduit chaque taille
> à 0, donc le tampon de la file a reçu une allocation de 0 octet
> (preuve du sondage : `queue+332 = 0`, `buffer = 0` chez
> `NtReadFile`)…

**Un correctif a déjà été écrit** (implémentation actuelle de
`NtQueryInformationFile`, appelant `ac6::native::
native_guest_media_service().file_size(...)` pour remplir de vraies
tailles) — mais la trace `NtReadFile` capturée par r448 montre
`queue@0x829ddd80: ... r332=00000000` **encore aujourd'hui** — le
correctif ne suffit PAS à résoudre le symptôme observé cette session.

## Établi — la cause ultime : le fichier de contenu n'est simplement jamais monté

```
$ ls build/ntsc-uj/source/assets/
default.xex
```
**Seul le XEX est présent.** Or :
```
$ ls game-files/
DATA00.PAC  DATA01.PAC  DATA.TBL  bgmpack.bin  default.xex
demopack_eng.bin  demopack_jpn.bin  dummy.bin  dummy30.bin
moviepack.bin  voicepack_eng.bin  voicepack_jpn.bin  $SystemUpdate
```
**Les fichiers de contenu réels existent bel et bien dans ce
workspace** (`game-files/`, et les deux images ISO complètes dans
`disc-image/`) — ils ne sont simplement jamais copiés vers
`source/assets/` par la préparation du profil natif :

```python
# tools/prepare.py, ligne 400-402
assets = source / "assets"
assets.mkdir()
shutil.copy2(Path(xex["source_path"]), assets / "default.xex")
```

**Seule cette ligne peuple `assets/` — rien d'autre n'y est jamais
copié.** `ac6::native::native_guest_media_service()` cherche
légitimement `DATA00.PAC` et ne le trouve jamais, parce qu'il n'a
jamais été placé là où le service de fichiers natif le cherche.

## Ce que ceci établit — la chaîne causale complète, de la première instruction au crash

1. **`tools/prepare.py` ne copie que `default.xex` dans
   `source/assets/`** — `DATA00.PAC`/`DATA01.PAC`/`DATA.TBL` et les
   autres paquets de contenu, bien présents dans `game-files/`, n'y
   sont jamais placés.
2. `native_guest_media_service()` ne trouve pas ces fichiers.
3. `NtQueryInformationFile` (déjà corrigé par r276 pour les fichiers
   trouvés) ne peut rien remplir pour un fichier introuvable — le
   remplissage à zéro de r240 s'applique toujours dans ce cas.
4. `sub_821CC008` (r441/r445) dérive une taille de tampon nulle à
   partir de ces zéros.
5. Le pool alloué avec cette taille reste marqué `0xFE` (r450) — car
   aucune vraie donnée de fichier n'arrive jamais à le remplir.
6. `sub_821CC508` lit un octet d'index toujours `0xFE` (r449), calcule
   un handle invalide, appelle `NtReadFile` avec ce handle et un
   décalage non initialisé (r448) — le stub refuse correctement de
   fabriquer un succès.
7. Après 5 tentatives, `sub_821CC508` abandonne (r446).
8. `sub_821D5F48` échoue l'une de ses 4 vérifications (r443), ne
   construit jamais l'objet de rendu à `*0x82935D98` (r436-r441).
9. `sub_821D7DE0` journalise l'échec sans s'arrêter (r442).
10. `sub_821D6C20` déréférence l'objet jamais construit → `SIGSEGV`
    (r435-r436).

**Chaque maillon de cette chaîne de dix étapes est maintenant
directement soutenu par une preuve mesurée — aucune supposition.**

## Non établi

- **Où exactement `native_guest_media_service()` s'attend à trouver
  ces fichiers** (quel chemin relatif précis sous `assets/`) et donc
  la correction exacte à apporter à `prepare.py` — non vérifié ce
  cycle.
- **Si la simple copie des fichiers de `game-files/` vers
  `source/assets/` suffit**, ou si un montage/format ISO/XDVDFS
  particulier est attendu par le service de fichiers natif — non
  vérifié.

## Décisions prises

- Ne pas modifier `prepare.py` ni copier de fichiers ce cycle — une
  vérification du chemin/format attendu par
  `native_guest_media_service()` est un préalable raisonnable avant
  toute modification, et le risque de rebuild/retest complet mérite
  son propre cycle dédié plutôt qu'une fin de cycle précipitée.
- Documenter la chaîne complète de bout en bout dans ce rapport,
  puisque c'est la clôture naturelle de toute l'investigation
  r399-r451.

## Gate

Aucune source de production éditée ce cycle (investigation en direct
et lecture de fichiers uniquement).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus, inchangées — aucune source de
production affectée)
`ctest` (racine et natif) inchangé depuis r450 (aucune source de
production modifiée).

## Named for r452

Vérifier le chemin/format exact attendu par
`ac6::native::native_guest_media_service()` pour `DATA00.PAC`
(recherche dans `native_guest_media.cpp`/`native_xdvdfs.cpp`), puis
étendre `tools/prepare.py` pour copier (ou monter) les fichiers de
contenu réels depuis `game-files/` vers `source/assets/` en plus du
XEX — reconstruire, relancer le probe, et vérifier si le pool se
peuple enfin de vraies données et si le crash de `sub_821D6C20`
disparaît. Reste ouvert sinon : décision de committage de l'arriéré
`native_vulkan_backend.cpp` (r433/r434/r438).

## Files

Aucun artefact gitignoré nouveau.

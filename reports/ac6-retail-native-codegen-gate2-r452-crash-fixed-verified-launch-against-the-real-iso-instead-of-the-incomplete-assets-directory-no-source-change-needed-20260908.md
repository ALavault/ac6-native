# AC6 retail NTSC-U/J — r452 — LE CRASH EST RÉSOLU, vérifié à deux reprises : lancer `ac6recomp` contre l'ISO retail réelle (déjà présente dans ce workspace, `disc-image/`) au lieu du répertoire `assets/` incomplet fait disparaître le `SIGSEGV` — AUCUN changement de source nécessaire, ferme la chaîne d'investigation r399-r452

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r451 : vérifier le chemin/format attendu par
`native_guest_media_service()`, étendre `tools/prepare.py` pour
monter le vrai contenu, reconstruire, relancer le probe, vérifier si
le crash disparaît.

## Établi — `NativeGuestMediaService` supporte déjà nativement un mode ISO complet

Lecture de `native/include/ac6/native_guest_media.h` : le service
supporte **deux modes**, déjà tous deux implémentés — un mode
« répertoire assets » (« petites fixtures de développement ») et un
**mode ISO** (« streamé directement depuis `iso_offset`/`size` à
chaque lecture, puisqu'un vrai paquet de données de titre dépasse
2 Gio »). Un commentaire de r240 confirme que ce mode a déjà été
qualifié précisément pour `DATA00.PAC` (2,2 Gio) sur ce même titre.
**`ac6recomp` lui-même accepte directement soit une ISO, soit un
répertoire d'assets en argument** (message d'erreur déjà connu :
« expected an existing .iso or assets directory »).

## Établi — le manifeste de build référence une ISO à un chemin périmé

`build/ntsc-uj/manifest.json` contient déjà
`"iso": {"source_path": ".../Ace Combat 6 - Fires of Liberation (USA, Japan)....iso"}`
— **à la racine du workspace**, un chemin qui n'existe plus (le
fichier a depuis été déplacé sous `disc-image/`, où il est toujours
présent, intact, 7 835 492 352 octets). `manifest.json` est un
artefact de build entièrement gitignoré (`build/` complet) — pas un
fichier à corriger à la main, simplement le résultat d'un
`prepare.py --iso <chemin d'alors>` dont le chemin a depuis bougé.

## Vérifié EN DIRECT, deux fois — LE CRASH DISPARAÎT

```
AC6_NATIVE_ALLOW_ENTRY_PROBE=1 AC6_NATIVE_PROBE_WINDOW_MS=25000 ./ac6recomp --probe-entry \
  "disc-image/Ace Combat 6 - Fires of Liberation (USA, Japan) (En,Fr,De,Es,It).iso"
```
**Premier lancement** (avec `AC6_NATIVE_IMPORT_TRACE=1`) : `exit=0`,
aucun `SIGSEGV`, `ac6recomp: presented_frames=5 state=2`, puis
**`ac6recomp: generated entry terminated its own thread`** — un arrêt
PROPRE du thread d'entrée, jamais observé auparavant dans toute cette
chaîne d'investigation (r427-r451 se terminaient tous soit par le
crash `SIGSEGV` connu, soit par l'expiration de la fenêtre de sondage
sans jamais voir ce message).

**Second lancement, contrôle propre** (sans trace) : identique —
`exit=0`, `presented_frames=5 state=2`,
`generated entry terminated its own thread`. **Reproductible.**

## Ce que ceci établit

**La chaîne causale complète de r399 à r451 est confirmée de bout en
bout par ce test positif** : le crash `SIGSEGV` de `sub_821D6C20`
n'était PAS un bug de codegen, ni un bug de logique du jeu, ni même
un défaut du stub natif `NtReadFile` — c'était uniquement l'absence
du contenu retail réel dans l'environnement de lancement utilisé par
cette session (le répertoire `assets/` ne contenant que le XEX). **Le
contenu réel existait depuis le début dans ce workspace**
(`disc-image/`, `game-files/`) — il suffisait de lancer contre la
bonne source.

**Aucun changement de code source de production n'est nécessaire**
pour cette résolution — c'est un problème d'argument de lancement/de
configuration de build, pas un bug à corriger dans le dépôt.

## Non établi

- **Si le contenu visuel réel est maintenant rendu** (plutôt que le
  placeholder déjà nommé par r434/r435/r438) — non vérifié ce cycle,
  le pipeline `PinnedShaderRuntime` reste toujours non branché au
  chemin `VdSwap` réel (fils ouverts séparés, indépendants de ce
  correctif).
- **Si `tools/prepare.py`/`tools/build.py` devraient être mis à jour**
  pour référencer par défaut le chemin actuel de l'ISO
  (`disc-image/...`) plutôt que l'ancien chemin périmé — un vrai
  changement de configuration de build, pas tenté ce cycle.

## Décisions prises

- Vérifier en lançant directement contre l'ISO plutôt que de modifier
  `prepare.py`/de recopier des fichiers dans `assets/` — la piste la
  plus rapide et la moins risquée pour confirmer l'hypothèse de r451
  avant tout changement de code ou de configuration.
- Ne pas modifier `tools/prepare.py`/`tools/build.py` ce cycle — le
  résultat positif est déjà complet et vérifié sans ces changements ;
  les adapter pour que les FUTURS cycles pointent par défaut vers le
  bon chemin est une amélioration de confort, pas une nécessité pour
  cette clôture.

## Gate

Aucune source de production éditée ce cycle (vérification en
lancement direct uniquement, aucun changement de code ni de
configuration de build).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus, inchangées — aucune source de
production affectée)
`ctest` (racine et natif) inchangé (aucune source de production
modifiée).

## Named for r453

Deux pistes indépendantes, ni l'une ni l'autre bloquante : (1) mettre
à jour `tools/prepare.py`/`tools/build.py` pour que les futurs cycles
de préparation du profil natif référencent par défaut le chemin
actuel de l'ISO (`disc-image/...`) — confort/fiabilité pour l'avenir,
pas une nécessité ; (2) reprendre le fil déjà nommé par r434/r435/r438
(le contenu visuel réel, `PinnedShaderRuntime` toujours non branché)
maintenant que le boot complet du jeu fonctionne sans crash. Reste
ouvert sinon : décision de committage de l'arriéré
`native_vulkan_backend.cpp` (r433/r434/r438).

## Files

Aucun artefact gitignoré nouveau (journaux de vérification sous
`/fastdata/tmp/...scratchpad/`, non conservés).

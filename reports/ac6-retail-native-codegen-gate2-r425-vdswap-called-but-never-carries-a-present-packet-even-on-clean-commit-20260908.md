# AC6 retail NTSC-U/J — r425 — `VdSwap` est bien appelé par le jeu invité, et son chemin de validation (`vd swap commit`) draine parfois du contenu réellement neuf de l'anneau — mais MÊME ALORS, le lot décodé ne contient jamais de paquet `Present`, malgré de vrais tirages (jusqu'à 50 dans un lot)

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r424 (nommé pour r425) : `presented_frames=0`/`present=0`
(r418, jamais résolu) — la boucle par image tourne à un rythme
réaliste mais le jeu invité n'émet toujours aucun paquet `Present`
observé dans la fenêtre de sonde.

## Établi

### `VdSwap` (le vrai appel noyau de présentation) est appelé — jamais vérifié auparavant dans cette chaîne

`r425_vdswap_check.gdb` (point d'observation sur `__imp__VdSwap`,
fenêtre `25000ms`) : **5 appels** en 25s, tous via la même pile
d'appel `sub_821F03B0<-sub_8234F558<-sub_8233E0A8<-sub_8233B5A0<-
sub_82331E78<-sub_821D7DE0` — le même chemin de boucle par image
soutenu déjà documenté par r417. `tools/materialize_native_import_
stubs.py` (stub `VdSwap`, commentaires `r296` déjà présents,
non ajoutés ce cycle) confirme que ce stub appelle
`native_guest_vd_service().publish_write_address(...)`.

### Le chemin de validation de `publish_write_address` (déjà instrumenté par r295/r296) draine parfois du contenu réellement neuf

`native/src/native_guest_vd.cpp` — commentaires `r295`/`r296` déjà
présents (non ajoutés ce cycle) documentent une inconnue antérieure :
« zero "vd swap commit" traces ever fire » à l'époque de leur
écriture. **Rejoué ce cycle (`AC6_NATIVE_VD_TRACE=1`, fenêtre
`25000ms`)** : sur les 5 appels `VdSwap`, **4 sont des no-op
silencieux** (`write_index == already_read`, rien de neuf), mais **1
déclenche réellement `vd swap commit`** :

```
vd swap check secondary=0x126c0e18 discovered=1 write_index=43 already_read=31
vd swap commit secondary=0x126c0e18 primary_write=43 read=31
vd drain accepted consumed_dwords=12
vd drain contents draw=4 resolve=0 present=0 wait=13 indirect=0 micro_init=0 event_write=4 immediate_shader=6 present_target_configured=1
```

**Le lot drainé contient bien du contenu réel (12 mots doubles, 4
tirages, 13 attentes, 4 écritures d'événement, 6 shaders immédiats) —
mais toujours AUCUN paquet `Present`.** Sur l'ensemble de la fenêtre,
un autre lot ultérieur (drainé par le sondeur d'arrière-plan, pas par
`VdSwap` lui-même) atteint même **50 tirages** — un rendu réel et
substantiel — sans jamais produire de `Present` non plus.

## Ce que ceci établit

**Ceci ferme définitivement l'hypothèse ouverte par r292-r296** (un
bug de scrutation/découverte empêchant `VdSwap` de jamais drainer de
contenu réellement neuf) : ce cycle montre un cas PROPRE où la
découverte réussit, où du contenu neuf existe, où le drainage a lieu
— et où, malgré cela, aucun opcode PM4 `XE_SWAP`
(`pm4::kOpcodeXeSwap`, requiert `payload[0]==kSwapSignature` et des
dimensions valides, `native/src/native_xenos.cpp:401`) n'apparaît
jamais dans le flux de commandes décodé. **Le jeu invité appelle bien
le noyau de présentation (`VdSwap`) et rend bien de la géométrie
réelle (jusqu'à 50 tirages), mais n'écrit jamais lui-même le paquet
PM4 `XE_SWAP` correspondant dans son propre tampon de commandes**,
dans la fenêtre de sonde observée — ou l'écrit sous une forme que le
décodeur PM4 actuel ne reconnaît pas encore comme telle.

## Non établi

- **Pourquoi le jeu invité n'écrit jamais le paquet `XE_SWAP`** — la
  fonction qui construirait ce paquet (probablement dans
  `sub_821F03B0` ou un appelant plus haut dans la même pile) n'a pas
  été lue. Deux hypothèses restent ouvertes, aucune vérifiée : (a) le
  jeu est encore dans une phase de boot/chargement où l'écriture du
  paquet de présentation est conditionnelle et cette condition n'est
  jamais remplie dans la fenêtre observée ; (b) le paquet est bien
  écrit mais sous une forme/signature que `native_xenos.cpp` ne décode
  pas encore comme `PresentPacket` (une divergence entre la structure
  PM4 réellement émise par ce binaire retail précis et celle attendue
  par le décodeur).
- **Si étendre la fenêtre de sonde au-delà de 25s** ferait
  éventuellement apparaître un paquet `Present` — non testé avec une
  fenêtre plus longue ce cycle.
- Aucun correctif proposé ni appliqué ce cycle.

## Décisions prises

- Vérifier d'abord si `VdSwap` est appelé DU TOUT avant de relire
  `sub_821D7AE0`/`sub_821D7CD0` en entier (nommés par r418) — une
  question plus étroite et moins coûteuse a suffi à progresser
  significativement sans cette lecture complète, laissée pour un
  cycle ultérieur si nécessaire (précédent CLAUDE.md/r413 : préférer
  la vérification la moins chère avant une lecture PPC complète).
- Ne pas deviner le mécanisme exact par lequel le paquet `XE_SWAP`
  serait censé être émis sans lire `sub_821F03B0` — nommé, pas conclu
  ici (précédent r1111/r1113).

## Gate

Aucune source de production éditée ce cycle.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées dans le commit de ce cycle)
`ctest` relancé en entier.

## Named for r426

Lire `sub_821F03B0` (l'appelant direct de `VdSwap`) en entier pour
localiser où/si le jeu construit un paquet PM4 `XE_SWAP` avant
d'appeler ce noyau — et si la structure qu'il écrit correspond bien à
ce que `native_xenos.cpp:401` (`kOpcodeXeSwap`, `kSwapSignature`)
attend. Si absent, remonter la pile (`sub_8234F558`, `sub_8233E0A8`,
`sub_8233B5A0`) pour trouver la condition qui gate son émission.

## Files

`recompilation/ace-combat-6-retail/artifacts/retail-us-native-r404-bucket17-writer/r425_vdswap_check.gdb/.log`,
`r425_vdswap_trace.log`.

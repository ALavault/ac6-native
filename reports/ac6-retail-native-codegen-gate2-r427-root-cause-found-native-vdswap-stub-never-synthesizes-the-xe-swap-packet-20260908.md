# AC6 retail NTSC-U/J — r427 — cause racine trouvée : le jeu invité ne construit JAMAIS lui-même le paquet PM4 `XE_SWAP` — il transmet ses paramètres de présentation en arguments à `VdSwap` en confiant au NOYAU le soin de l'injecter dans l'anneau ; notre stub hôte de `VdSwap` ignore ces arguments et ne synthétise rien

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r426 (nommé pour r427) : lire `sub_821F03B0` (l'appelant
direct de `VdSwap`) ligne par ligne pour localiser soit la condition
qui gate l'émission du paquet `XE_SWAP`, soit ce qui bloque le jeu
avant son premier swap réel.

## Établi

### Le jeu réserve de l'espace dans l'anneau, mais n'y écrit jamais de paquet PM4 lui-même

`sub_821F03B0` (1015 lignes), juste avant son appel à `VdSwap`
(`ppc_recomp.26.cpp`, extrait vers `0x821F0618`-`0x821F0664`) :

1. `__imp__VdGetSystemCommandBuffer(ctx.r4=r1+116, ...)` — obtient un
   descripteur du tampon de commandes système.
2. `sub_821E4F88(r3=r31/périphérique, r4=64)` — appelé juste après.
3. **`sub_821E4F88`, lu en entier** (`ppc_recomp.26.cpp:1577`) :
   vérifie simplement s'il reste `64` octets de marge entre le
   curseur d'écriture courant (`+48(r31)`) et la limite de l'anneau
   (`+52(r31)`), appelle `sub_821E60A8`/`sub_821E5650` (repli si
   pleine) au besoin, et **retourne le curseur courant SANS RIEN
   ÉCRIRE DANS LE TAMPON**. C'est un allocateur d'espace dans l'anneau,
   pas un constructeur de paquet.
4. Le résultat (`r30`) est ensuite utilisé directement comme argument
   de `VdSwap` (`ctx.r3 = r30+4`), aux côtés de six autres arguments
   (`r4`..`r10`, pointeurs vers de petites structures sur la pile —
   format probable : tampon frontal, palette, dimensions) — **aucune
   écriture PM4 (`PPC_STORE_U32` d'un en-tête de type 3) n'apparaît
   nulle part entre la réservation et l'appel `VdSwap`.**

**Le jeu invité transmet donc ses paramètres de présentation
directement en ARGUMENTS à l'appel noyau `VdSwap`, en confiant au
NOYAU (pas à lui-même) la construction et l'injection du paquet PM4
`XE_SWAP` dans l'anneau.** C'est cohérent avec le fonctionnement réel
d'un appel système Xbox 360 : sur le matériel réel, `VdSwap` est
implémenté PAR LE NOYAU xboxkrnl.exe, qui construit ce paquet à partir
de ses arguments — ce n'est jamais du code du jeu qui l'écrit
lui-même.

### Notre stub hôte de `VdSwap` ne fait que de la comptabilité — il ne construit jamais ce paquet

`tools/materialize_native_import_stubs.py` (déjà cité par r425/r426,
relu ce cycle) : **le corps entier du stub `VdSwap`** est :

```c++
ac6::native::native_guest_vd_service().publish_write_address(
    base, ctx.r3.u32);
ctx.r3.u64 = 0u;
```

**Seul `ctx.r3` (le curseur réservé) est lu — les six autres
arguments (`ctx.r4`..`ctx.r10`, portant le tampon frontal, la palette,
les dimensions) sont intégralement ignorés.** `publish_write_address`
(déjà lu par r418/r425/r426) ne fait QUE de la comptabilité de
curseur (« y a-t-il du contenu neuf entre `already_read` et
`write_index` ? ») — elle ne synthétise ni n'injecte jamais de paquet
PM4 dans l'anneau.

## Ce que ceci établit

**Cause racine complète et cohérente avec CHAQUE observation de
r418-r426** : `presented_frames` reste à `0` non pas parce que le jeu
invité est bloqué, en attente, ou mal formé — mais parce que
**l'implémentation hôte du noyau `VdSwap` (r191-ère, jamais revue
depuis) est incomplète : elle ne fait pas le travail que le vrai
noyau Xbox 360 ferait**, à savoir construire le paquet PM4 `XE_SWAP`
(`native_xenos.cpp:401`, `kOpcodeXeSwap`/`kSwapSignature`) à partir
des sept arguments que le jeu lui transmet fidèlement à chaque appel,
et l'injecter dans l'anneau à l'emplacement réservé. Ceci n'est PAS
un défaut du jeu retail ni de la chaîne allocateur/threads r399-r422 —
c'est une lacune d'implémentation, localisée, dans le stub hôte
lui-même.

## Non établi

- **La sémantique exacte des sept arguments** (`r3`..`r10`) — quel
  registre porte le pointeur de tampon frontal, la palette, les
  dimensions — n'a pas été confirmée en détail (les structures
  pointées par `r1+96/100/104/108/116/128/208` n'ont pas été
  décortiquées champ par champ).
- **Le correctif exact** — synthétiser et écrire un paquet PM4
  `XE_SWAP` valide dans le stub `VdSwap`, à partir de ces arguments,
  au curseur réservé — n'a été ni conçu ni appliqué ce cycle.
- **Si ceci résoudrait `presented_frames=0` de bout en bout** (le
  reste du pipeline — `present_to_offscreen`, la capture d'écran —
  n'a pas été revérifié pour ce cas précis).

## Décisions prises

- Lire `sub_821E4F88` en entier dès que son rôle est devenu ambigu
  (« réserve-t-il de l'espace ou écrit-il aussi le paquet ? ») plutôt
  que de deviner — a directement révélé qu'il ne fait AUCUNE écriture,
  la pièce manquante décisive de ce cycle.
- Ne pas concevoir ni appliquer de correctif de synthèse PM4 sans
  d'abord confirmer la sémantique exacte des sept arguments — nommé
  pour un cycle dédié plutôt que deviné (précédent r1111/r1113).

## Gate

Aucune source de production éditée ce cycle.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées dans le commit de ce cycle)
`ctest` relancé en entier.

## Named for r428

Décortiquer les sept arguments de `VdSwap` (`ctx.r3`..`ctx.r10`) —
lire les structures pointées sur la pile dans `sub_821F03B0` pour
identifier lequel porte l'adresse du tampon frontal et les
dimensions — puis concevoir, appliquer et vérifier en direct un
correctif du stub `VdSwap` qui synthétise réellement un paquet PM4
`XE_SWAP` et l'injecte à l'emplacement réservé avant d'appeler
`publish_write_address`.

## Files

Aucun artefact gitignoré ce cycle (lecture de code source
uniquement).

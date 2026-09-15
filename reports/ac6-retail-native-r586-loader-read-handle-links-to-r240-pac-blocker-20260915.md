# r586 — Le stall de complétion (r585) rejoint le blocage PAC pré-existant (r240/r241) : `read_file` rejette le handle FILE_OBJECT du chargeur

## Qualification

- **Ghidra project** : `ghidra-projects/ace-combat-6`, `default.xex` (retail
  `ntsc-uj`). Analyse statique côté natif (`recompilation/ace-combat-6-retail/
  native/`) + décompilation (r585).
- **Oracle** : non. Aucun budget N3.
- **Aucun run ce cycle** : GPU saturé par `neural_amp` (r584). Analyse
  statique ; aucun patch runtime commité (non validable sous contention).

## Ce que ce cycle établit

r585 a montré que le mode MissionTitle gèle parce que la **lecture asynchrone du
nœud de chargement ne se termine jamais** (`Function_82345C88` ré-arme sans fin ;
la `step` du nœud reste 0). Ce cycle suit le fil côté natif et le relie à un
blocage **déjà documenté et jamais résolu**.

1. **`NtReadFile` est synchrone et sans événement** (stub généré, r124/r125) : il
   lit `r7=&IoStatusBlock`, `r8=Buffer`, `r9=Length`, `r10=&ByteOffset`, appelle
   `native_guest_media_service().read_file(handle, offset, dest, len,
   bytes_read)`, écrit `status`+`bytes_read` dans l'IoStatusBlock et renvoie.
   `STATUS_INVALID_HANDLE (0xC0000008)` si le handle est inconnu ;
   `STATUS_END_OF_FILE` si 0 octet lu sur une longueur non nulle ;
   `STATUS_SUCCESS` sinon.

2. **`read_file` est un lookup strict par handle** (`native_guest_media.cpp:80`) :
   ```cpp
   const auto it = files_.find(handle);
   if (it == files_.end()) { bytes_read = 0u; return false; }  // -> INVALID_HANDLE
   ```
   `files_` est indexé par le **petit HANDLE** rendu par
   `NtCreateFile`/`NtOpenFile`.

3. **Le chemin PAC passe un mauvais handle** — établi de longue date, cité dans le
   commentaire `NtReadFile` du générateur (r240/r241) : « the PAC path still
   fails with handle=0x829xxxxx (image FILE_OBJECT, not small HANDLE) and
   offset=0xfefefefe (stack garbage, not 0) ». Un `handle` à `0x829xxxxx` n'est
   pas dans `files_` → `read_file` renvoie `false` → `INVALID_HANDLE`, 0 octet.

## L'hypothèse de liaison (forte, non encore confirmée par un run)

MissionTitle déclenche le chargement des assets de mission (PAC). Si ce
chargement lit via le chemin PAC identifié en r240/r241, alors chaque lecture du
nœud échoue au niveau du handle (`0x829xxxxx` absent de `files_`), la pompe
`Function_82345C88` ne reçoit jamais d'octets terminaux, `Function_821D2860`
reste 0, et le mode n'avance jamais vers Briefing — exactement le gel de r585.

**Pourquoi « hypothèse » et non « établi »** : je n'ai pas de trace runtime
`[NtReadFile]` **au mode MissionTitle** montrant le handle/offset réels de la
lecture qui gèle. Le boot franchit logos/titre/menus, donc *certaines* lectures
réussissent (handles valides) ; il reste à prouver que la lecture bloquante à
MissionTitle est bien une lecture PAC à handle FILE_OBJECT. La confirmation
demande un run tracé (`AC6_NATIVE_IMPORT_TRACE` borné au mode MissionTitle) —
impossible sous saturation GPU, et le trace non borné avait produit 73 Go (r583).

## Décisions prises (en place d'une question)

1. **Relier r585 à r240/r241 dans un rapport** plutôt que traiter le stall de
   complétion comme un problème neuf : le mécanisme natif (rejet de handle dans
   `read_file`) est un candidat direct et déjà connu.
2. **Ne pas patcher `read_file`/le modèle de handle à l'aveugle** : sans run de
   validation, faire accepter `0x829xxxxx` (ou remapper le FILE_OBJECT vers son
   petit handle) serait une règle sans contrôle — refusée. Il faut d'abord
   confirmer que c'est bien cette lecture qui gèle.

## Prochaine barrière (précise)

1. **Run tracé borné au mode MissionTitle** : loguer un seul `[NtReadFile]`
   (handle, offset, len, IoStatusBlock) par la lecture du nœud de chargement,
   pour confirmer le handle `0x829xxxxx` et l'offset. Nécessite GPU non saturé.
2. Si confirmé : déterminer où le chargeur PAC obtient son handle
   (`NtCreateFile`/`NtOpenFile` renvoie-t-il un petit handle jamais propagé, ou
   le chargeur utilise-t-il directement un FILE_OBJECT image ?), puis faire que
   `read_file` résolve ce handle vers le bon fichier streamé — et **valider** que
   le run avance MissionTitle → Briefing (`0x8206360C`) puis vers le vol.

## Résumé de la campagne r584→r586 (mur restant documenté)

- **r584** : MissionTitle atteint (première fois, sampler armé) ; gel ; blocage
  GPU documenté (`neural_amp` 100 % / 19.3 Go).
- **r585** : cause = lecture async du chargeur qui ne se termine jamais ; modèle
  producteur/consommateur de r497 corrigé (mauvaise attribution).
- **r586** : mécanisme natif candidat = `read_file` rejette le handle FILE_OBJECT
  du chargeur PAC (r240/r241, pré-existant). Confirmation + fix = run requis,
  bloqué par la saturation GPU.

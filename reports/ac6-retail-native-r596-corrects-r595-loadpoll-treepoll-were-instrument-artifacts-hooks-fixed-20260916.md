# r596 — Corrige r595 : `f54`/`realkey`/`list1c`/`f20`/`flag24` étaient tous des ARTEFACTS d'instrument (gardes d'adresse + mauvaise cellule) ; hooks corrigés

## Qualification

- **Ghidra project** : `ghidra-projects/ace-combat-6`, `default.xex` (retail
  `ntsc-uj`). Oracle : non.
- **Preuve** : exports Ghidra faisant autorité (`exports/821b8430.json`,
  `821b9408.json`, `821d2860.json`, `821d2ad8.json`) + cross-match recomp
  (`ppc_recomp.21.cpp:7738`) via sous-agent Explore ; lecture du process gelé
  vivant (pid 3823195) **impossible** (`/proc/<pid>/mem` refusé sous
  `ptrace_scope=1` pour un non-ancêtre, sudo non disponible). Aucun run neuf.

## Ce que r595 a mal dit (correction par preuve)

r595 a conclu que la readiness poll de MissionTitle « n'est jamais prête » avec
`f54=fefefefe` (non initialisé) et `realkey=0`. **Faux — ce sont des artefacts
de mesure**, pas des valeurs guest :

1. **Sémantique réelle de la poll** (`exports/821b8430.json`,
   `Function_821B8430`, param = `ls`) : renvoie prêt ssi `*(ls+0xc)==0`
   (state==0), **ou** state==1 ET le tree-walk `Function_821D2860` renvoie ≠0.
   La clé de lookup est **`*(mgr+0x54)`** (ou `+0x58` selon le flag `ls+0x11`),
   `mgr = *(0x8293B930)`. **La poll ne lit JAMAIS `ls+0x54`.**
2. **`f54=fefefefe`** : le hook loggait `ls+0x54` — un champ que la poll ne lit
   pas ; `0xfefefefe` est du padding non pertinent.
3. **`realkey=0`** : le hook lisait `dat` depuis `0x8293BA10` (nom Ghidra-DB)
   alors que l'image exécutée lit `0x8293B930` (décalage uniforme +0xE0 ;
   `ppc_recomp.21.cpp:7738`). **Et** sa garde `dat>=0x82000000` rejetait de
   toute façon `mgr=0x188d0000` (tas). Le lookup correctement instrumenté (r589,
   lecture des vrais args) donnait déjà `key=b362294c ret=18a30200` : **la clé
   existe et le nœud existe**. `res=0` n'est donc PAS une clé nulle ni un nœud
   manquant.
4. **`list1c=0 f20=0 flag24=0`** (treepoll) : contradiction directe avec
   `exports/821d2860.json` — quand les deux listes (`node+0x20`, `node+0x1c`)
   sont vides, le walk fait `*(node+0x24)=0; return 1` (**PRÊT**). Or le hook
   loggait `ret=0` avec listes « vides ». Cause : le helper `gw` du hook ne
   lisait que `[0x82000000,0x90000000)` et **le nœud est sur le tas
   (0x18a30200)** → `gw` renvoyait 0 pour **tous** les champs du nœud. Les zéros
   n'étaient pas mesurés, ils étaient refusés par la garde.

**Bilan** : les trois « valeurs » sur lesquelles r595 s'appuyait pour dire « le
loader n'est pas prêt / non initialisé » sont des artefacts. La seule chose
établie reste : le mode atteint MissionTitle (0x82065064) au frame 1702 puis le
thread game-loop (tid 3823197) **spinne en userspace** (r595, /proc — cela tient).

## Ce que r595 a AUSSI surestimé (noté par l'advisor)

- « spinne sur la loadpoll » : déduit du **dernier hook loggé**, pas mesuré. Les
  hooks sont gated et le spin n'émet rien. L'IP exact n'est pas épinglé
  (`ptrace_scope=1` bloque gdb ET `/proc/mem`).
- La démotion du ring PM4 (r595) ne couvre que le frame-gate d'**Opening**
  (updates 1304→1701 pendant le wedge) ; elle **ne réfute pas** que le gate de
  MissionTitle lise un slot que seul le tail EVENT_WRITE de l'IB coinçant écrit.
  Cette piste reste ouverte, non écartée.

## Correctif d'instrument (contrôlé, scaffolding local — pas de fix guest)

`recompilation/ace-combat-6-retail/native/src/ac6recomp_main.cpp` :
- loadpoll (`sub_821B8478`) : `dat` lu depuis **`0x8293B930`** (pas 0x8293BA10) ;
  garde `dat>=0x82000000` → `dat!=0` (le tas est valide ; le mapping guest 4 Gio
  rend toute adresse 32 bits lisible).
- treepoll (`sub_821D28C8`) : helper `gw` `[0x82000000,0x90000000)` → `a!=0`,
  pour lire les champs de nœud sur le tas (0x18xxxxxx). Corrige `list1c/f20/
  flag24` et le walk de nœuds.

Build `ac6recomp` OK. C'est une réparation d'instrument prouvé faux
(« mesurer l'instrument avant de s'y fier », CLAUDE.md), pas une règle sans
contrôle : les adresses correctes viennent du cross-match d'export.

## Non établi (dit clairement) — la vraie question, désormais mesurable

- Les vraies valeurs de `mgr+0x54`, `node(0x18a30200)+0x1c/+0x20/+0x24`, et le
  retour réel de `Function_821D2860` : **non lues** (mem live bloquée). Un run
  neuf avec les hooks corrigés les donnera.
- Deux issues possibles, que la mesure corrigée tranchera : (a) listes non vides
  → le walk renvoie 0 sur un enfant dont `vtable[0x14]()` renvoie 0 (chargement
  asynchrone jamais fini) ; (b) listes vides → le walk renvoie 1 (prêt) et le
  `res=0` venait d'un autre nœud/chemin → le gel est ailleurs (revenir au gate
  MissionTitle / piste ring non écartée).
- L'IP du spin de tid 3823197 (loadpoll vs frame-gate `sub_821E61A8` vs autre).

## Prochaine barrière (précise)

Relancer un run VD_TRACE + BOOT_PHASE_TRACE avec les hooks corrigés jusqu'à
MissionTitle ; lire `realkey` (attendu ~b362294c), et `treepoll list1c/f20/
flag24` réels + `treepoll-node` (classe du nœud bloquant). Trancher (a) vs (b).
C'est la couche 2 du plan (MissionTitle → Briefing).

## Décisions prises

1. **Corriger r595 (mon cycle précédent, ce jour)** : `f54`/`realkey`/`list1c`/
   `f20`/`flag24` = artefacts (mauvaise cellule +0xE0 + gardes d'adresse
   rejetant le tas), pas des valeurs guest ; la poll ne lit pas `ls+0x54`.
2. **Réparer l'instrument** avec les adresses du cross-match d'export, plutôt
   que d'interpréter des zéros refusés par une garde.
3. **Ne rien deviner sur le guest** : les vraies valeurs restent non lues (mem
   live bloquée par `ptrace_scope=1`) ; nommées comme prochaine mesure sur un
   run neuf. Ce qui tient de r595 : atteinte de MissionTitle + spin userspace du
   game-loop.

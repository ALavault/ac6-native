# AC6 retail NTSC-U/J — r440 — `sub_821D6C20` et `sub_821D7DE0` désassemblées et décompilées dans le bon projet Ghidra (`ac6-us`) : preuve solide que `*0x82935D98` est le manager de périphérique/rendu du jeu — jamais construit dans cet environnement offline ; nouvel outil réutilisable committé

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r439, piste 1 : désassembler réellement `sub_821D6C20` dans
le bon projet Ghidra (`ac6-us`) pour permettre une recherche statique
plus poussée. Le script d'amorçage existant
(`SeedPdataExtentDisassembly.java`) est verrouillé au mauvais SHA
(`acc302c1...`) et fait une reconstruction de tout le binaire (via
`.pdata`, 65 968 octets) — disproportionné pour deux fonctions. Un
nouveau script ciblé, verrouillé au bon SHA, a été écrit à la place.

## Outil ajouté

`scripts/DecompileD6C20Dispatcher.java` — désassemble et décompile
`sub_821D6C20` et `sub_821D7DE0`, bornées par les propres points de
coupure de fonction du codegen retail qualifié (la prochaine
`PPC_FUNC_IMPL` après chacune, faute de ligne `.pdata` pour l'une ou
l'autre) plutôt que par `.pdata`. Verrouillé au SHA-256 qualifié
(`6eefba42...`, PAS `acc302c1...` comme les autres scripts de ce
dossier). Committé pour réutilisation future — deux fonctions
seulement, borné, lisible.

**Effet sur le projet Ghidra local** : lancé SANS `-readOnly` (pour
que le décompilateur ait quelque chose à analyser) — crée ces deux
fonctions dans `ghidra-projects/ac6-us` (répertoire entièrement
gitignoré, local, réversible ; aucune autre fonction touchée, aucun
effet sur le dépôt git). Justification de ce choix par rapport à
`-readOnly` : documentée dans le script lui-même.

## Établi — décompilation en direct

### `sub_821D7DE0` est la boucle principale du jeu (ou son point d'entrée immédiat)

```c
void Function_821D7DE0(void) {
  func_0x82382a1c();
  cVar2 = Function_821D5F48();
  if (cVar2 == '\0') { func_0x821f5b18(0xffffffff82054b48, 0); }  // assert-fail si échec
  Function_82331DE8(); Function_82331D90(7); Function_82331E30();
  iVar3 = 2;
  do { Function_82331E78(); Function_82331DE8(); Function_82331D90(7);
       Function_82331E30(); iVar3 = iVar3 + -1; } while (iVar3 != 0);
  cVar2 = Function_821D6C20();
  if (cVar2 == '\0') { func_0x821f5b18(0xffffffff82054b48, 0); }  // assert-fail si échec
  Function_82331E78(); Function_82331DE8(); Function_82331D90(7); Function_82331E30();
  do {                                            // BOUCLE INFINIE — boucle de jeu
    Function_821D7AE0(); Function_821D7CD0();
    (**(code **)(*piRam82671308 + 0xc))();
    (**(code **)(*piRam8269dc90 + 0x2c))();
    Function_82331E78();
    /* compteur de trame iRam826e4dd0, remis à 0 tous les 2 tours */
    Function_82331DE8(); Function_82331E30();
  } while( true );
}
```

**Ce que ceci établit précisément** : `sub_821D6C20` est un pas
d'initialisation **« doit réussir »** (motif identique à
`sub_821D5F48` juste avant lui : échec → `func_0x821f5b18` — un
gestionnaire d'échec/assertion) appelé **une seule fois, juste avant
que la boucle infinie de jeu ne commence**. C'est exactement la
position d'un appel d'initialisation de sous-système critique dans un
programme de ce type (D3D/Xenos/périphérique de rendu, typiquement),
pas un dispatcher générique répété par trame.

### `sub_821D6C20` déréférence `piRam82935d98` de façon totalement non gardée, comme SA TOUTE PREMIÈRE action

```c
undefined8 Function_821D6C20(void) {
  func_0x82382a10();
  (**(code **)(*piRam82935d98 + 0xe4))();   // <-- LE SITE DE CRASH, aucune garde avant
  if ((iRam8293b950 != 0) && (bRam829ddeb9 < 8)) { ... }
  ...
```

Aucune vérification de nullité avant ce premier appel — cohérent avec
du code retail réel où l'objet est censé être **déjà construit** par
un point antérieur du démarrage, pas testé défensivement ici.

### Le corps de la fonction, une fois ce premier appel dépassé, ressemble fortement à une CONFIGURATION de périphérique de rendu à partir de valeurs de réglages indexées

Le reste du corps (branche de succès, ~35 appels) suit un motif très
régulier : `uVar4 = func_0x82234d40(0xffffffff829ddd88, N)` (lecture
d'une valeur de configuration à l'index `N`, `N` de `0` à `0x29`),
immédiatement suivi d'un appel à travers la vtable de `piRam82935d98`
(offsets `0x13c`, `0x138`, `0x158`, `0x15c`, `0xc`) ou d'objets liés
(`piRam823f9b28`, treize pointeurs `iRam826a00b4`..`e4` tous appelés à
`+8`, puis `piRam827be1d0` avec des drapeaux `0x400`/`0x40`/`0x200`/
`0x100`). **Ce motif — lire une table de réglages indexée puis
pousser chaque valeur dans un objet de périphérique via sa vtable,
suivi de drapeaux de capacité en fin de fonction — est exactement la
forme d'une initialisation de périphérique de rendu/GPU à partir des
réglages du titre**, pas un dispatcher d'événements générique.

## Ce que ceci établit

**Hypothèse forte, bien plus précise que r436-r439** : `*0x82935D98`
est très probablement le manager de périphérique de rendu/GPU du jeu
(ou un objet de rôle équivalent — device Xenos, contexte D3D, ou
analogue) — pas un « objet quelconque jamais construit », un objet
dont TOUT LE RESTE de cette fonction (la quasi-totalité de son corps)
n'a de sens que s'il s'agit de la configuration d'un périphérique de
rendu. Ceci est cohérent avec le fil déjà nommé par r425 (« VdSwap
appelé mais ne porte jamais de PresentPacket même sur un commit
propre ») et r434/r435 (le contenu visuel réel n'a jamais été
brancé) — **ces trois fils pourraient converger vers la même cause
racine plus large : le pipeline de rendu réel du jeu (au niveau
titre, pas seulement le stub hôte `VdSwap`) ne s'initialise jamais
complètement dans cet environnement offline.**

## Non établi

- **La fonction/le stub hôte exact censé construire cet objet** —
  toujours pas trouvé (aucune écriture nulle part dans le code
  atteignable compilé, confirmé par r437/r439 par deux méthodes
  indépendantes). L'hypothèse « manager de rendu » réduit l'espace de
  recherche (chercher du côté d'un stub hôte lié au périphérique
  graphique/Xenos plutôt que n'importe où) mais ne le résout pas.
- **Le lien exact avec r425/r434/r435** — hypothèse de convergence
  plausible, pas vérifiée par une trace ou une preuve croisée ce
  cycle.

## Décisions prises

- Écrire un script ciblé plutôt que réutiliser/adapter
  `SeedPdataExtentDisassembly.java` — deux fonctions bornées par les
  points de coupure du codegen suffisent au besoin, pas la
  reconstruction complète du binaire.
- Committer le script (`scripts/`, suivi par git, sans rapport avec
  l'arriéré `native_vulkan_backend.cpp`) mais ne rien committer du
  projet Ghidra local (`ghidra-projects/`, gitignoré par conception).
- Ne pas pousser plus loin l'identification exacte de l'objet ce
  cycle (par exemple décompiler `sub_821D5F48` ou les fonctions
  `func_0x82234d40`/`Function_821CC008`/`Function_821CC508` pour
  confirmer le sous-système de réglages) — l'hypothèse actuelle est
  déjà bien plus précise et actionnable que ce qui existait avant ce
  cycle ; la pousser davantage relève d'un cycle dédié.

## Gate

**Source committée ce cycle** : `scripts/DecompileD6C20Dispatcher.java`
(nouvel outil, aucune source de production touchée).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus, inchangées — aucune source de
production affectée)
`ctest` (racine et natif) inchangé depuis r439 (aucune source de
production modifiée).

## Named for r441

Confirmer l'hypothèse « manager de périphérique de rendu » en
décompilant `sub_821D5F48` (l'étape d'initialisation juste avant,
également « doit réussir ») et les fonctions de lecture de réglages
(`func_0x82234d40` et voisines) avec le même script/méthode élargi —
et si cela pointe vers un stub hôte Vulkan/Xenos manquant ou
incomplet plutôt qu'un simple ordre d'exécution, relier explicitement
à r425/r434/r435. Reste ouvert sinon : la décision de committage de
l'arriéré `native_vulkan_backend.cpp` (r433/r434/r438).

## Files

Nouveau : `scripts/DecompileD6C20Dispatcher.java` (committé). Aucun
artefact gitignoré nouveau sous `reports/`.

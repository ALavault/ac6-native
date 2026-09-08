# AC6 retail NTSC-U/J — r442 — correction du cadrage « doit réussir » de r440/r441 : `sub_821F5B18` (appelé quand `sub_821D5F48` échoue) est un classifieur de chaîne NON FATAL, pas un abandon — le boot continue de toute façon, cohérent avec le fait établi que `sub_821D5F48` renvoie un échec sans jamais écrire `*0x82935D98`, tout en laissant `sub_821D6C20` s'exécuter quand même et planter

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r441 : tracer en direct quel chemin de retour
`sub_821D5F48` emprunte réellement, et quelle condition le sélectionne.

## Établi

### `sub_821D5F48` n'a que DEUX points de sortie réels dans le source généré, pas plusieurs indépendants

`grep "^\treturn;"` sur la plage `[0x821D5F48, 0x821D6C1C)` du source
généré (`ppc_recomp.23.cpp`) : exactement **2** instructions
`return;` — une très tôt dans la fonction (ligne relative 288,
`loc_821D6138`, avec `r3=0` juste avant), une très tard (ligne
relative 1703, juste après le bloc d'écriture de `*0x82935D98`
identifié par r441, avec `r3=1`). **4 sites distincts**
(`goto loc_821D6138` à des points variés, jusqu'à la ligne 20046 —
très proche du bloc d'écriture à la ligne 20257) convergent tous vers
le même point de sortie précoce.

### Le point d'arrêt de r441 sur l'instruction d'écriture ne s'étant jamais déclenché, et la fonction retournant sans planter ni bloquer, la seule sortie possible restante est le retour précoce (`r3=0`, échec)

Puisqu'il n'existe que deux sorties, que l'une (l'écriture, r3=1) est
prouvée non atteinte (r441, point d'arrêt en direct), et que la
fonction retourne effectivement (le boot continue normalement vers
`sub_821D6C20` sans blocage ni crash à ce stade) — **par élimination,
`sub_821D5F48` retourne `0` (échec) dans ce run**, sans jamais
construire `*0x82935D98`.

### `sub_821F5B18` (appelé par `sub_821D7DE0` quand `sub_821D5F48` échoue) est un classifieur de chaîne, pas un abandon fatal

Lecture du corps généré de `sub_821F5B18` : une boucle de comparaison
octet par octet entre la chaîne passée en argument et deux chaînes de
référence statiques (motif `strcmp`), ajustant un fanion dans `r4`
selon le résultat — **aucune instruction d'arrêt, de `TerminateThread`,
ni de branchement vers un gestionnaire d'exception**. C'est une
fonction de classification/journalisation de message d'erreur, pas un
`assert()` fatal.

### Confirmation structurelle déjà présente dans la décompilation de r440, non relevée alors

Le corps décompilé de `sub_821D7DE0` (r440) montre que l'appel à
`func_0x821f5b18` se trouve DANS un simple `if`, **sans aucun `return`
ni branchement de sortie après** — le flux continue inconditionnellement
vers les instructions suivantes (`Function_82331DE8(); ...`), qu'il
soit entré dans le `if` ou non. **Le cadrage « doit réussir »
utilisé par r440/r441 était donc une légère surinterprétation** : le
code appelle un gestionnaire d'erreur/log en cas d'échec, mais rien
n'empêche le boot de continuer — pas un arrêt conditionnel réel.

## Ce que ceci établit

**La chaîne causale complète, maintenant cohérente de bout en bout,
sans aucune contradiction résiduelle** :
1. `sub_821D5F48` (bootstrap Xenos/renderer, confirmé par r441) échoue
   quelque part dans cet environnement offline (l'une des 4
   conditions convergeant vers `loc_821D6138`), retourne `0`, **sans
   jamais écrire `*0x82935D98`**.
2. `sub_821D7DE0` appelle `sub_821F5B18` pour journaliser l'échec —
   **un log, pas un arrêt** — puis continue sans condition.
3. `sub_821D6C20` est appelé quand même, déréférence
   `*0x82935D98` (toujours nul) sans aucune garde → `SIGSEGV`.

**Ce n'est plus une énigme de « pourquoi le processus ne s'arrête-t-il
pas » — la réponse est que rien dans ce code n'était censé s'arrêter
sur cet échec. Le vrai défaut reste en amont : pourquoi l'une des 4
conditions de `sub_821D5F48` échoue dans cet environnement offline.**

## Non établi

- **Laquelle des 4 conditions (lignes 18900, 19153, 19629, 20046 du
  source généré) échoue réellement** — le réordonnancement du code
  compilé x86 (constaté en tentant de localiser précisément
  `loc_821D6138` en direct) rend la corrélation adresse-hôte ↔
  adresse-invité plus coûteuse que prévu pour ce cycle ; non résolu.
- **La nature exacte de ces 4 conditions** (quelles fonctions elles
  appellent, ce qu'elles vérifient) — non décompilées ce cycle.

## Décisions prises

- Ne pas pousser la corrélation précise adresse-hôte ↔ adresse-invité
  pour identifier LAQUELLE des 4 conditions échoue — le
  réordonnancement du code compilé rend cela plus coûteux qu'anticipé
  (l'instruction `ret` unique et le motif observé montrent que le
  compilateur a réorganisé les blocs, invalidant l'hypothèse simple
  « position textuelle précoce = adresse hôte précoce ») ; nommé pour
  un cycle séparé plutôt que forcé ce cycle.
- Corriger le cadrage « doit réussir » de r440/r441 dans ce rapport
  plutôt que de le laisser tel quel — la relecture de la
  décompilation déjà en main (r440) suffisait à voir l'absence de
  branchement de sortie après l'appel de log, une fois que la question
  s'est posée directement.

## Gate

Aucune source de production éditée ce cycle (investigation en direct
et lecture de source généré uniquement).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus, inchangées — aucune source de
production affectée)
`ctest` (racine et natif) inchangé depuis r441 (aucune source de
production modifiée).

## Named for r443

Identifier laquelle des 4 conditions convergeant vers
`loc_821D6138` échoue réellement dans cet environnement offline (les
quatre sites sont aux lignes 18900, 19153, 19629 et 20046 du source
généré — celui de la ligne 20046, à ~200 lignes du bloc d'écriture,
est le plus proche et donc le suspect le plus économique à vérifier
en premier). Nécessitera une corrélation adresse-hôte ↔ adresse-invité
plus rigoureuse que la recherche textuelle utilisée jusqu'ici (le
réordonnancement du code compilé l'a invalidée pour ce cycle) —
probablement via un point d'arrêt sur chacun des appels PRÉCÉDANT
chaque `goto`, dont les cibles sont, elles, faciles à localiser en
direct (comme `sub_820B1CD0`/`sub_82338300` l'ont été ce cycle et le
précédent). Reste ouvert sinon : la décision de committage de
l'arriéré `native_vulkan_backend.cpp` (r433/r434/r438).

## Files

Aucun artefact gitignoré nouveau.

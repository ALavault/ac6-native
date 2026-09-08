# AC6 retail NTSC-U/J — r450 — `0xFE` n'est PAS de la mémoire jamais initialisée : c'est un `memset` DÉLIBÉRÉ (`sub_823830F0`, confirmé être un vrai `memset`), écrit exactement une fois, vu EN DIRECT (point d'arrêt matériel depuis le début du process) — la vraie question devient : pourquoi rien ne le réécrit ensuite avec de vraies données

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r449 : identifier quelle étape devrait peupler le pool
(`0x173b0000`-région) avec de vraies données avant que
`sub_821CC508` ne le lise.

## Correction de cadrage majeure, établie EN DIRECT

r449 avait interprété le motif `0xFE` uniforme comme « mémoire jamais
peuplée ». Un point d'arrêt matériel posé sur l'octet exact
(adresse invité `0x173b0038`) depuis la toute première instruction du
process (même technique que r437) **s'est déclenché** :

```
Old value = 0 '\000'
New value = 254 '\376'
0x0000555555f99f2b in __imp__sub_823830F0 ()
#0  __imp__sub_823830F0 ()
#1  __imp__sub_821D5F48 ()
#2  __imp__sub_821D7DE0 ()
#3  __imp___xstart ()
```

**Ce n'est PAS de la mémoire jamais écrite** — elle valait `0`
(fraîchement allouée/zéro-remplie) puis a été explicitement mise à
`0xFE` par un appel depuis `sub_821D5F48` lui-même.

## Établi — `sub_823830F0` est un vrai `memset`

Décompilation de `sub_823830F0` (`0x823830F0`-`0x82383180`) :
motif classique et non ambigu d'implémentation optimisée de
`memset(param_1, param_2, param_3)` — boucle d'alignement octet par
octet, puis remplissage par mots de 4 octets, puis octets de queue.
**`sub_821D5F48` appelle explicitement `memset(pool_ptr, 0xFE,
taille)` sur ce pool juste après l'avoir alloué.**

## Ce que ceci établit

**`0xFE` est un motif de « poison »/sentinelle DÉLIBÉRÉ, pas un oubli
d'initialisation** — un `memset` de nettoyage/marquage après
allocation est un motif défensif tout à fait normal (rendre visible
toute utilisation de données pas-encore-réelles). **Ce n'est donc pas
en soi un bug** : c'est le comportement ATTENDU juste après
l'allocation. **La vraie question se déplace encore une fois en
amont** : sur un vrai matériel, quelque chose doit réécrire ce pool
avec de vraies données (un vrai index de fichier/handle) AVANT que
`sub_821CC508` ne le lise — et dans cet environnement offline, cette
réécriture n'a manifestement jamais lieu (l'octet vaut encore `0xFE`
au moment de la lecture par `sub_821CC508`, confirmé par r449).

Cohérent avec toute la chaîne déjà établie (r441-r449) : le motif
général — allouer un pool, le marquer en attente
(`STATUS_PENDING`=0x103 déjà vu, ici `memset(0xFE)`), puis attendre
qu'une opération asynchrone le peuple — est exactement la forme d'un
pipeline de chargement de contenu asynchrone (PAC), dont le
déclencheur ou l'achèvement ne se produit jamais dans cet
environnement offline.

## Non établi

- **Quel code est censé réécrire ce pool** avec de vraies données, et
  **quel événement/déclencheur** devrait l'initier (peut-être lié au
  mécanisme de file d'événements déjà vu, `sub_821D4988`, r445, ou à
  un chargement de contenu asynchrone séparé jamais amorcé dans cet
  environnement).
- **Si le déclencheur manquant est un stub hôte natif incomplet**
  (par exemple un événement/notification que ce dépôt ne simule pas)
  **ou une ressource de contenu absente** (le fichier PAC concerné
  n'existe simplement pas dans les assets montés).

## Décisions prises

- Vérifier EN DIRECT plutôt que de se fier à l'interprétation
  statique du motif de r449 — un point d'arrêt matériel a suffi à
  distinguer « jamais écrit » de « écrit une fois, délibérément » à
  faible coût, cohérent avec la discipline de ce dépôt de toujours
  vérifier avant de conclure.
- Décompiler `sub_823830F0` pour confirmer sans ambiguïté qu'il
  s'agit d'un `memset` générique plutôt que de deviner à partir du
  seul nom d'adresse.

## Gate

Aucune source de production éditée ce cycle (investigation en direct
uniquement ; le script `DecompileD03D5CDispatchTarget.java`, déjà
committé par r448, a été réutilisé avec une nouvelle plage
d'adresses — pas un nouveau fichier).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus, inchangées — aucune source de
production affectée)
`ctest` (racine et natif) inchangé depuis r449 (aucune source de
production modifiée).

## Named for r451

Tracer ce qui devrait réécrire le pool `0x173b0000`-région après le
`memset(0xFE)` initial — chercher tout code qui écrit dans cette
plage APRÈS l'appel de `sub_823830F0` (un deuxième point d'arrêt
matériel sur le même octet, laissé armé plus longtemps, dirait s'il
n'est JAMAIS réécrit avant le crash, confirmant définitivement
l'absence totale de deuxième écrivain plutôt que juste son absence
avant `sub_821CC508`). Reste ouvert sinon : décision de committage de
l'arriéré `native_vulkan_backend.cpp` (r433/r434/r438).

## Files

Aucun artefact gitignoré nouveau. Le script
`scripts/DecompileD03D5CDispatchTarget.java` (committé par r448) a
été réutilisé sur place avec une adresse différente pour ce cycle —
son contenu final (adressant `sub_823830F0`) n'a pas été recommitté
séparément puisqu'il s'agit du même outil réutilisable, pas d'un
nouveau script à committer.

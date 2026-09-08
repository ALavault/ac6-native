# AC6 retail NTSC-U/J — r447 — `0x13D` tracé jusqu'à du VRAI CODE INVITÉ (pas un stub hôte manquant) : `sub_821F4E70` fait un dispatch indirect à travers un objet de périphérique réel, résolu en direct jusqu'à `sub_823CFE08` — recadrage important de l'hypothèse « stub natif incomplet »

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r446 : tracer plus profondément `sub_821F4E70` (la
« lecture » elle-même) au-delà de son helper de prologue générique,
pour trouver le vrai stub hôte natif responsable de `0x13D`.

## Établi

### `sub_821F50A0` est un simple thunk vers `sub_821F75F0`

Lecture directe du source généré : `sub_821F50A0` (l'équivalent
`GetLastError`) n'est qu'un branchement inconditionnel vers
`sub_821F75F0` — un vrai stub d'indirection PPC, pas une fonction
propre.

### `sub_821F4E70` ne passe PAS par un stub hôte natif — c'est un dispatch indirect via un objet noyau réel

Lecture complète du source généré : `sub_821F4E70` initialise une
structure de type bloc de statut NT (`*(r31+0) = 259` — `0x103` =
`STATUS_PENDING`, motif NT classique), puis calcule une adresse fixe
`0x823F07CC` (`.data`) et **charge un POINTEUR d'objet à cette
adresse**, puis lit un DEUXIÈME pointeur à `+16` de cet objet, et
fait un appel indirect (`mtctr`/`bctrl`) à travers lui — exactement
le même motif de dispatch vtable-comme déjà vu partout dans cette
chaîne (`piRam82935d98+0xe4`, etc.), mais ICI appliqué à un objet
périphérique/fichier plausible.

### Résolu EN DIRECT, pas de manière statique : la cible réelle est du vrai code invité, pas un stub hôte

Lecture en direct (processus arrêté au point d'entrée de
`sub_821F4E70`) :
- `*0x823F07CC` (objet) = `0x823F07A0` (une vraie adresse `.data`,
  après correction de l'ordre des octets big-endian).
- `*(0x823F07A0 + 16)` (cible de dispatch) = **`0x823D035C`** —
  **dans la plage `.text` (0x82090000-0x823d772b), une vraie adresse
  de code invité, pas une adresse de stub hôte.**

`0x823D035C` tombe à l'intérieur de la fonction générée
`sub_823CFE08` (`0x823CFE08`-`0x823D0E00`, ~4 Ko), pas décompilée ce
cycle.

## Ce que ceci établit

**Recadrage important** : `0x13D` n'est PAS injecté par un stub hôte
incomplet dans ce dépôt (aucun stub natif n'apparaît nulle part dans
cette chaîne d'appels) — **c'est du VRAI CODE PPC RETAIL RECOMPILÉ,
dispatché à travers un objet noyau/périphérique dont le pointeur
lui-même est correctement résolu, qui calcule cette valeur**.
L'hypothèse à privilégier change en conséquence : ce n'est probablement
pas un stub natif manquant, mais soit **une condition/donnée réelle**
que ce code invité teste et qui diffère dans cet environnement
offline (par exemple, un état de média/disque, une région de
partition, un indicateur de configuration), soit **un bug de codegen
dans la traduction de `sub_823CFE08`** (moins probable — le motif de
dispatch vtable lui-même a déjà été vérifié fidèle à plusieurs
reprises cette session, mais pas encore vérifié pour CETTE fonction
précise).

## Non établi

- **Le contenu de `sub_823CFE08`** — non décompilé ce cycle, c'est
  cette fonction qui calcule réellement `0x13D`.
- **Quelle donnée/condition précise** cette fonction teste, et
  pourquoi elle diffère de ce qu'un vrai matériel produirait.

## Décisions prises

- Résoudre la cible du dispatch EN DIRECT plutôt que de deviner
  statiquement — la lecture en direct de deux niveaux de pointeurs a
  confirmé sans ambiguïté que la cible est du code invité réel, pas
  une supposition à partir de la seule lecture du désassemblage.
- Ne pas décompiler `sub_823CFE08` ce cycle — fonction de ~4 Ko, un
  travail de décompilation et de lecture substantiel mérite son
  propre cycle plutôt que d'être précipité en fin de cycle.

## Gate

Aucune source de production éditée ce cycle (investigation en direct
uniquement).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus, inchangées — aucune source de
production affectée)
`ctest` (racine et natif) inchangé depuis r446 (aucune source de
production modifiée).

## Named for r448

Décompiler `sub_823CFE08` (bornes `0x823CFE08`-`0x823D0E00`, cible
réelle du dispatch résolue à `0x823D035C`, à l'intérieur) pour
comprendre précisément ce qui produit `0x13D`, et déterminer si c'est
une condition de données légitimement différente dans cet
environnement offline ou un bug de codegen localisé à cette fonction.
Reste ouvert sinon : décision de committage de l'arriéré
`native_vulkan_backend.cpp` (r433/r434/r438).

## Files

Aucun artefact gitignoré nouveau.

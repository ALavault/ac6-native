# AC6 retail NTSC-U/J — r449 — la garbage remonte jusqu'à un VRAI POOL MÉMOIRE dont la structure de chaînage (liste libre) est valide mais dont le CONTENU/PAYLOAD n'a jamais été rempli — motif de remplissage `0xFE` uniforme, pas un bug de calcul d'index

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r448 : désassemblage statique autour du site d'appel réel
de `NtReadFile` à l'intérieur de `sub_821CC508`/`sub_821D5F48`, pour
déterminer si le handle/décalage incorrects viennent d'un bug de
codegen ou d'une étape d'initialisation manquante.

## Établi

### Le « handle » vient d'une lecture indexée signée, pas d'une variable directe

Source généré, juste avant l'appel à `sub_821F4E70` (le vrai site
d'appel `NtReadFile`, confirmé en direct) :
```c
r11 = *(int8_t*)(r30 + 0);              // lbz + extsb : octet signé
r11 = (r11 << 2) & 0xFFFFFFFC;          // rlwinm : × 4, sur 32 bits
ctx.r3 = *(uint32_t*)(r11 + r24);       // lwzx : TABLE[index signé]
```
**Si l'octet lu est négatif (bit de poids fort posé), l'index devient
négatif et la lecture se fait AVANT le début de la table** — motif
confirmé exactement dans le désassemblage x86 compilé (calcul du
signe via `test`/`cmovs`, puis `lea`/`and` pour l'adressage indexé).

### Vérifié EN DIRECT : l'octet d'index est `0xFE` (-2 signé), pas une valeur de contrôle valide

Point d'arrêt exactement sur la lecture de cet octet : adresse invité
`0x173b0038`, valeur `0xFE`. **`0x173b00XX` correspond exactement au
`pool=0x173b0028` déjà vu dans la trace `NtReadFile` de r448** — le
même pool mémoire, confirmé sans ambiguïté.

### Le pool est un VRAI pool alloué, avec une structure de chaînage valide — mais son contenu n'a jamais été rempli

Vidage mémoire de 64 octets autour de cette adresse :
```
0x7fff0cbb0018:  fe fe fe fe fe fe fe fe
0x7fff0cbb0020:  17 3b 00 30  fe fe fe fe
0x7fff0cbb0028:  fe fe fe fe fe fe fe fe
0x7fff0cbb0030:  17 3b 00 40  fe fe fe fe
0x7fff0cbb0038:  fe fe fe fe fe fe fe fe
0x7fff0cbb0040:  17 3b 00 50  fe fe fe fe
...
```
**Motif clair et cohérent** : des blocs de 16 octets, chacun contenant
un pointeur valide vers le bloc SUIVANT du même pool
(`0x173b0030`→`0x173b0040`→`0x173b0050`→…, une vraie chaîne de liste
libre bien formée), entourés d'octets de remplissage **`0xFE`
uniformes**. **La structure d'allocation du pool elle-même est
correcte** — ce n'est pas un pool corrompu ou mal alloué — **mais son
contenu utile (payload) n'a jamais été écrit avec de vraies données**.

## Ce que ceci établit

**Ce n'est PAS un bug de codegen dans le calcul de l'index ou de
l'adresse** — la traduction PPC→x86 de la lecture indexée signée est
fidèle (vérifiée dans les deux sens, source généré et désassemblage
compilé). **C'est un pool mémoire réel, correctement alloué et
chaîné, dont le contenu (y compris l'octet servant d'index de
sélection de fichier) n'a jamais été peuplé par de vraies données** —
cohérent avec `offset=0xFEFEFEFE` (r448) : le même motif de
remplissage uniforme `0xFE` apparaît partout dans cette structure,
pas seulement à un seul octet isolé.

**Ceci pointe vers une étape de population de données manquante**,
probablement liée à la lecture réelle du contenu PAC/fichier
(cohérent avec la terminologie déjà utilisée par r240/r241/r275 et
avec les fonctions d'énumération de fichiers déjà identifiées par
r441/r445-r446) — pas un défaut de traduction du code lui-même.

## Non établi

- **Quelle étape antérieure devrait peupler ce pool** avec de vraies
  données (un index de fichier valide plutôt que le remplissage
  `0xFE`) — non tracée ce cycle.
- **Si cette étape manquante est un stub hôte natif incomplet**
  (par exemple, une fonction qui devrait copier des données lues
  d'un fichier réel dans ce pool, mais dont ce dépôt n'a pas encore
  l'implémentation) **ou une condition de disponibilité de contenu**
  (le fichier/paquet PAC concerné n'existe pas dans les assets montés
  par cet environnement offline).

## Décisions prises

- Vérifier la valeur ET son contexte mémoire environnant plutôt que
  de s'arrêter à la première lecture — le motif de remplissage
  uniforme `0xFE` n'aurait pas été visible avec une seule lecture
  d'octet, et c'est ce motif qui distingue clairement « donnée jamais
  peuplée » de « bug de calcul isolé ».
- Ne pas deviner quelle étape devrait peupler ce pool sans preuve
  supplémentaire — nommé pour un cycle séparé.

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
`ctest` (racine et natif) inchangé depuis r448 (aucune source de
production modifiée).

## Named for r450

Identifier quelle étape (native ou invitée) devrait peupler ce pool
(`0x173b0000`-région) avec de vraies données avant que
`sub_821CC508` ne le lise — chercher les écrivains de cette plage
mémoire (probablement liée à `sub_821CC008`/l'énumération de
fichiers déjà caractérisée, ou à un stub hôte de lecture PAC encore
incomplet). Reste ouvert sinon : décision de committage de l'arriéré
`native_vulkan_backend.cpp` (r433/r434/r438).

## Files

Aucun artefact gitignoré nouveau.

# AC6 retail NTSC-U/J — r409 — corrige r408 (`seq=348` était une écriture de comptabilité intermédiaire, pas un free anormal) ET localise en direct l'origine réelle et sa première conséquence concrète : un reliquat de découpe écrit `0x823f` unités là où `3` étaient dues (`seq=8`), et le tout premier chevauchement mémoire confirmé du run survient à `seq=284` — 590 appels avant la chaîne r399-r408

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

**Ce rapport a été réécrit deux fois avant publication.** Une première
version attribuait la divergence de r408c à une "perturbation de
minutage" (réfutée par relecture attentive du journal). Une seconde
version identifiait mal le bloc victime de la découpe fautive
(comparé au mauvais nœud). Voir "Corrections" puis "Établi" pour le
détail vérifié.

## Contexte

Suite de r408 (nommé pour r409, étape 3) : capturer le RETOUR de
chaque appel `sub_821F9E10` pour vérifier l'anomalie "free avant
alloc" que r408 avait notée à `seq=348`.

## Corrections (retirent une partie de r408 et d'une version antérieure de ce rapport)

1. **`seq=348` n'est pas un free anormal.** Deux captures indépendantes
   (`r409_returns.log`, `r406_ordering.log`, toutes deux
   `total-seq=883`, concordantes événement par événement de `seq=340`
   à `359`) montrent que `seq=348` libère `0x10002590`, pas
   `0x10080000`. Le déclenchement du point d'observation matériel de
   r408c à `seq=348` (pile `sub_821F94F8 <- sub_821FA6F8 <- ...`) se
   produit PENDANT ce free, comme effet de bord d'une routine de
   comptabilité de coalescence — **explication maintenant établie, pas
   supposée : voir point 5 de "Établi".**
2. **Le bloc victime de la découpe fautive N'EST PAS le nœud de ~37 Ko
   observé à `seq=8`** (celui-ci reste inchangé entre `seq=8` et
   `seq=9`, confirmé par comparaison directe des deux instantanés).
   L'identification correcte, dérivée de la formule de reliquat déjà
   lue par r405 (`r30 = r3 + (r29<<4)`), est au point 2 de "Établi".

## Établi

### 1. Le nœud fantôme existe déjà à `seq=10`, inchangé jusqu'à `seq=100`

`r410b_bracket.gdb` (parcours de l'arbre des grands blocs à plusieurs
points de contrôle) : un second nœud, `hdr=0x100007c0`,
`size_units=0x823f` (`0x823f×16=0x823F0` octets = 533 488), span
`[0x100007c0,0x10082bb0)` — identique à chaque point de contrôle de
`seq=10` à `seq=100`. Sa borne haute, `0x10082bb0`, est à `0x110`
octets de `0x10082aa0` — l'adresse au centre de toute l'enquête
r399-r408.

### 2. Le nœud fantôme est créé à `seq=8`, en découpant le bloc alloué à `seq=2` et libéré à `seq=6`

`r410_early_state.gdb`/`r410b_bracket.gdb` : arbre propre à `seq=5`
(nœud unique `[0x10001590,0x10010000)`, exactement la commission
initiale — infirme directement l'hypothèse "seedé avec la réserve
entière" envisagée par une version antérieure de ce fil). Points de
contrôle serrés (`5,6,7,8,9,10`) : arbre propre après le 7ᵉ appel
(état "`seq=8`"), nœud fantôme déjà présent après le 8ᵉ appel (état
"`seq=9`"). **Le nœud fantôme est donc créé par l'exécution de l'appel
`#8` lui-même** (`ALLOC-ENTER seq=8 r4=0x0 r5=0x50`, 80 octets).

Application de la formule de reliquat (r405, `loc_821FA1D4`) :
`r29 = ((0x50+31)&~15)>>4 = 6` unités demandées ; header du bloc trouvé
`= 0x100007c0 − (6×16) = 0x10000760`, pointeur `0x10000770`. **Cette
adresse est exactement celle retournée par `ALLOC-RET seq=2 r4=0x8
r5=0x80 -> r3=0x10000770`** (`r409_returns.log`), un bloc de 128
octets, libéré ensuite par `FREE seq=6 r5=0x10000770`. **Le bloc
trouvé et découpé par `seq=8` fait donc `9` unités
(`((0x80+31)&~15)>>4`) ; une requête de `6` unités devrait laisser un
reliquat de `3` unités (48 octets) — le reliquat réellement écrit fait
`0x823f` unités, pas `3`.**

### 3. L'en-tête est correct AVANT `seq=8` ; l'erreur naît pendant le traitement de la découpe elle-même

`r412_verify.gdb` (lecture directe de l'en-tête du bloc trouvé, juste
avant que l'appel `#8` ne s'exécute) : `u16[0x10000760]=0x9` — **le
champ de taille vaut exactement `9` unités, correct**, et le bloc est
déjà proprement inséré dans le bucket 9 (`heap+0x1c8` (bucket exact
pour 9 unités) `[0]=[4]=0x10000768`, une liste circulaire à un seul
élément valide). **L'en-tête n'est donc PAS corrompu avant `seq=8` —
l'erreur naît pendant le traitement de CETTE découpe.**

`r411_pin_writer.gdb` (point d'observation matériel armé SEULEMENT à
l'entrée de l'appel `#8`) : quatre déclenchements, tous à `seq=8`, tous
dans `sub_821F9E10` — trois sur le champ de taille (`0x100007c0`,
offsets hôte `...ce21`/`...ce53`/`...d119`), un sur le champ de
chaînage (`0x100007c8`, offset `...d3c9`). **Ce motif (plusieurs
écritures `u8`/`u16` successives dans le même mot de 32 bits) est celui
attendu de `loc_821FA1D4` déjà lu par r405 (`stb +5`, `sth +2`,
`stb +4`, `sth +0`) — il ne discrimine PAS, à lui seul, si la valeur
finalement écrite (`0x823f`) vient d'un mauvais registre `r6` en entrée
de ce code ou d'une erreur DANS son calcul. Voir "Non établi".

### 4. Première conséquence concrète confirmée : chevauchement mémoire à `seq=284`, 590 appels avant la chaîne r399

`ALLOC-ENTER seq=284 r4=0x0 r5=0x8c` (140 octets) retourne
`ALLOC-RET seq=284 ... -> r3=0x100007d0` (`r409_returns.log`) — la
PREMIÈRE découpe qui consomme le nœud fantôme, produisant un bloc
utilisé `[0x100007c0, 0x100007c0+16+0x8c) = [0x100007c0,0x10000860)`.
**`0x10000800` (le pointeur retourné à `seq=3`, `ALLOC-RET seq=3 r4=0x8
r5=0x580 -> r3=0x10000800`, un bloc de 1408 octets) n'est libéré à
AUCUN moment du run entier** (recherché dans l'intégralité de
`r409_returns.log`, aucune occurrence de `FREE ... r5=0x10000800`).
**Le bloc retourné à `seq=284` (`[0x100007c0,0x10000860)`) chevauche
donc physiquement le bloc encore vivant retourné à `seq=3`
(`[0x100007f0,0x10000d80)`, calcul exact
`0x100007f0+16+0x580=0x10000d80`, jamais libéré) — le premier chevauchement
mémoire confirmé de tout le run, 590 appels avant l'invocation de
`sub_821F9E10` que r404 avait documentée comme "la" collision
(`seq=874`).**

### 5. À `seq=349`, le nœud `0x10080000` porte la MÊME signature "à moitié inséré" que r403 avait documentée pour `0x10082aa0`

Comparaison des instantanés `seq=348`→`seq=349` (`r410c_bracket_narrow.log`) :
le nœud unique observé à `seq=348` (`[0x100025d0,0x10082bb0)`, le
reliquat du nœud fantôme rétréci par les découpes successives depuis
`seq=284`) devient DEUX nœuds au prochain point de contrôle :
`[0x10002490,0x10010000)` (qui se termine EXACTEMENT à la frontière de
commission, `0x10010000`) et `[0x10080000,0x10082bb0)`.
`sub_821F94F8` (lu partiellement, appelée depuis `sub_821FA6F8` au free
de `seq=348`) effectue un arrondi à 64 Ko (`rlwinm ...,0,0,15`)
cohérent avec ces deux frontières.

**Lecture directe des champs de chaînage à `seq=349`**
(`r412_verify.gdb`) : `bucket0_header(0x10000180)[0]=0x10080008` (le
lien AVANT pointe bien vers le nœud `0x10080000`) mais
**`node2(0x10080000)[4]=0x0`** — **son propre lien retour est NUL.**
C'est exactement la signature "insertion à moitié faite" que r403 avait
documentée pour `0x10082aa0`, des dizaines de rapports plus tard dans
ce fil — ici confirmée, en direct, sur un nœud DIFFÉRENT et bien plus
tôt dans le run. **Ce n'est donc pas un mécanisme défensif du tas qui
"protège" la frontière de commission** (hypothèse envisagée puis
écartée) — c'est la MÊME anomalie de chaînage que r403/r404 documentent
plus tard, qui se reproduit ici pour la première fois observée dans ce
fil.

### 6. Corroboration indépendante : le compteur d'unités libres du tas passe négatif

`heap+0x30` (compteur déjà identifié par r403 comme décrémenté à
chaque retrait de bucket) : `0x2d9` à `seq=200`, **`0xffffff85`
(négatif en 32 bits signés) à `seq=300`** (`r410b_bracket.log`) — le
tas lui-même détecte, par un signal totalement indépendant du parcours
d'arbre, qu'il a distribué plus de mémoire qu'il ne pense en avoir eu —
cohérent avec, et confirmant indépendamment, le chevauchement établi au
point 4.

## Ce que ceci établit pour r399

**La chaîne entière r399-r408 documentait les symptômes tardifs d'une
seule erreur survenue à `seq=8` : la découpe d'un bloc de 9 unités pour
une requête de 6 unités a écrit un reliquat de `0x823f` unités au lieu
de `3`.** Ni le mécanisme de croissance (`sub_821F92B8`, dont la
comptabilité de segment ne montre aucune activité avant `seq=876`,
r407/r409) ni un "double-octroi entre deux invocations tardives"
(r404/r406) n'en sont la cause — ce sont des conséquences, bien plus
tardives, du même nœud fantôme se faisant progressivement découper.
Le premier dommage concret (chevauchement mémoire, pas seulement
comptabilité incohérente) est confirmé à `seq=284`.

## Non établi

- **La ligne PPC exacte des trois écritures du champ de taille** —
  les adresses hôte n'ont pas encore été converties en labels
  `loc_821FAxxx` par comptage d'instructions (technique r403).
- **Pourquoi la découpe écrit `0x823f` au lieu de `3`** — un registre
  ou un champ mémoire contient déjà cette valeur avant l'écriture,
  mais sa source n'est pas tracée. Aucune hypothèse retenue sans
  capture directe de ce registre.
- **Le rôle exact du drapeau `r4`** (`0x0` pour `seq=7`-`9`,
  `0x8` pour `seq=1`-`5`) dans `sub_821F9E10`. Vérifié ce cycle : `r28`
  (initialement suspecté comme porteur de ce drapeau) est en fait une
  CONSTANTE interne (`li r28,1`, utilisée pour un masque de bitmap),
  sans rapport avec l'argument `r4` appelant — piste écartée. `r4`
  (nommé `r29` dans le corps de la fonction) est bien lu au prologue et
  combiné par OR avec `heap+24` dans `r23`
  (`r23 = (heap+24) | r29`), mais l'usage de `r23` en aval n'a pas été
  tracé.
- **Ce que fait la chaîne d'appel `sub_8234F2C8 <- sub_8233E1D8 <-
  sub_82346B48 <- sub_821DE8D8`** (jamais vue avant ce cycle dans ce
  fil) — aucune de ces fonctions n'est lue.
- L'ampleur exacte des dommages ultérieurs entre `seq=284` et `seq=874`
  (combien d'autres chevauchements, quels blocs) — seul le premier est
  confirmé ici.

## Décisions prises

- Retirer la conclusion "perturbation de minutage" et corriger
  l'identification du bloc victime dès qu'une relecture/un calcul plus
  attentifs les ont contredites, plutôt que de laisser une conclusion
  fausse entrer dans `NEXT.md` deux fois de suite dans le même cycle.
- Poursuivre avec des points d'observation matériel bornés dans le
  temps (armés à l'entrée de l'appel ciblé, pas depuis le début du
  processus) après l'incident de r408c.
- Ne pas deviner la ligne PPC exacte ni la source de `0x823f` sans les
  tracer — nommées pour r410, pas conclues ici (précédent r1111/r1113).

## Gate

Aucune source de production éditée ce cycle.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF -> exit 0
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json -> exit 0
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json -> exit 0
```
`ctest` relancé en entier : même échec préexistant et sans rapport que
r404 (`ac6-cpp-complexity`), 87/88 (99%).

## Named for r410

1. Convertir les trois adresses hôte des écritures du champ de taille
   (`...ce21`/`...ce53`/`...d119`) en labels PPC par comptage
   d'instructions depuis l'entrée connue de `sub_821F9E10` (technique
   r403), pour identifier laquelle des trois écrit `0x823f` et
   pourquoi.
2. Capturer en direct la valeur du registre/champ juste avant chacune
   des trois écritures, pour tracer `0x823f` jusqu'à sa source.
3. Lire `sub_8234F2C8`/`sub_8233E1D8`/`sub_82346B48`/`sub_821DE8D8`
   (chaîne d'appel de `seq=8`, jamais vue dans ce fil) si la trace du
   point 2 ne suffit pas à elle seule.

## Files

`recompilation/ace-combat-6-retail/artifacts/retail-us-native-r404-bucket17-writer/r409_returns.gdb/.log`,
`gdb-stdout-r409.log`, `r410_early_state.gdb/.log`, `gdb-stdout-r410.log`,
`r410b_bracket.gdb`, `r410c_bracket_narrow.log`, `gdb-stdout-r410c.log`,
`r410d_bracket_tight.log`, `gdb-stdout-r410d.log`,
`r411_pin_writer.gdb/.log`, `gdb-stdout-r411.log`,
`r412_verify.gdb/.log`, `gdb-stdout-r412.log`.

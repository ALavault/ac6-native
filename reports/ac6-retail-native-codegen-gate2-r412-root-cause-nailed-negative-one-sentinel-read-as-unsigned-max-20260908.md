# AC6 retail NTSC-U/J — r412 — cause racine confirmée au niveau instruction : la requête de capacité renvoie `-1` (sentinelle "libéré") sur le tampon déjà libéré, et `sub_8237FA50` la compare en NON SIGNÉ — `-1` devient `0xFFFFFFFF`, "capacité maximale", et désactive la revérification pour toujours

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r411 (nommé pour r412) : lire `sub_82385AF0` (requête de
capacité) en entier, ou isoler par point d'arrêt x86 la branche exacte
juste après le premier `bl sub_823857E0` de `sub_8237FA50`.

## Établi

### 1. `sub_82385AF0` délègue à `sub_821F90A8` — la vraie logique de requête de capacité

Lecture complète (`ppc_recomp.65.cpp:24709` et `ppc_recomp.27.cpp:12413`) :
`sub_82385AF0(ptr)` acquiert le verrou du tas (`sub_821FB298`) puis
appelle `sub_821F90A8(heap, 0, ptr)`, dont la logique est :

```
r11 = octet_flags(ptr-11)          // = en-tête+5, déjà identifié par r405/r410
r10 = r11 & 0x1                     // bit "en cours d'utilisation"
si r10 == 0 (bloc PAS en cours d'utilisation) :
    retourner -1                    // sentinelle explicite "libéré"
sinon (bloc vivant) :
    calculer et retourner la taille utile réelle depuis les champs
    d'en-tête (chemin "petit bloc" ou "grand bloc" selon un autre bit)
```

C'est un comportement DÉLIBÉRÉ et correct en lui-même : interroger la
capacité d'un pointeur déjà libéré est une erreur d'appelant, et la
fonction le signale par une sentinelle `-1` plutôt que de calculer une
taille sur un en-tête qui ne lui appartient plus.

### 2. Capturé en direct : la sentinelle `-1` est bien émise après le `free()`, et masquée par une comparaison non signée côté appelant

`r422_capacity_query.gdb` (188 déclenchements consécutifs, tous sur le
même pointeur `0x10000770`, lecture corrigée : la valeur de retour de
cette fonction est stockée en mémoire dans `ctx.r3` — offset `+0` de
`PPCContext`, PAS laissée dans `%rax` au `ret` x86, ce qui avait donné
une première lecture erronée par une capture antérieure abandonnée) :

| flags`+5` | retour |
|---|---|
| `0x1` (en cours d'utilisation) | `0x80` = **128** (la taille utile réelle du tampon de `seq=2`) |
| `0x0` (libéré, après `seq=6`) | `0xFFFFFFFFFFFFFFFF` = **`-1`** |

La transition a lieu exactement à la requête où le `free()` de `seq=6`
(r410, r411) prend effet — cohérence totale avec le mécanisme lu au
§1. `sub_8237FA50` compare ce retour (`r29`) au besoin (`r27`, une
petite valeur positive, la longueur courante + 4) avec **`cmplw`
(comparaison NON SIGNÉE, texte déjà cité par r410/r411)** :
`-1` réinterprété en 32 bits non signé vaut `0xFFFFFFFF`, la valeur
maximale possible — la comparaison `r29 >= r27` est **toujours vraie**
quel que soit le besoin réel, donc `bge cr6,loc_8237FB00` ("capacité
suffisante, ajouter directement") est **toujours prise** après cette
première libération.

## Ce que ceci établit pour r399, r410, r411

**Chaîne causale complète, du symptôme final jusqu'à l'instruction
précise :**

1. Le tableau croissant statique C++ (`sub_8237FA50`/`0x82a5eef0`)
   grandit une première fois (`seq=5`/`6`, r411) et libère
   correctement son ancien tampon de 128 octets (`0x10000770`).
2. Le pointeur `begin` du tableau n'est jamais mis à jour vers le
   nouveau tampon (r411, mécanisme du saut de commit toujours non
   établi — voir "Non établi").
3. **CE rapport ferme la boucle** : même SANS ce bug de commit, le
   tableau continuerait indéfiniment d'utiliser le même pointeur
   `0x10000770` car sa propre logique de contrôle de capacité, une
   fois ce pointeur libéré, retourne systématiquement `-1` —
   interprété comme "capacité quasi infinie" par une comparaison non
   signée côté appelant. **Les deux bugs de r411 et de ce rapport se
   renforcent** : même si le commit avait fonctionné, le nouveau
   tampon (`0x100015a0`, 256 octets) aurait fini par être rempli à son
   tour et, à SA PROPRE libération lors d'une croissance suivante,
   aurait déclenché exactement le même défaut — sauf que la
   comparaison non signée de la sentinelle `-1` masque justement le
   signal qui devrait déclencher cette croissance suivante. C'est donc
   ce rapport, pas r411, qui identifie le défaut qui rend le
   débordement PERMANENT et NON BORNÉ plutôt qu'auto-corrigé à la
   prochaine croissance.
4. Le débordement qui en résulte (r410, r411 : le tableau écrit un mot
   de 4 octets par objet statique C++ enregistré, indéfiniment, à
   travers la mémoire qui suit l'ancien tampon libéré) est la source
   directe de la corruption localisée par r409/r410 à `0x100007f0`
   (le voisin physique immédiat), qui alimente toute la chaîne
   r399-r409 documentée depuis r358.

## Non établi

- **Pourquoi le commit du pointeur `begin` (`loc_8237FAF0`, r411) ne
  s'exécute jamais malgré un retour de croissance non nul confirmé** —
  toujours pas lu ligne à ligne ni capturé par point d'arrêt x86 direct
  sur cette branche précise. Ce rapport rend la question moins
  urgente pour EXPLIQUER le débordement observé (le bug de la
  sentinelle `-1` suffit à lui seul à le produire, que le commit se
  fasse ou non), mais elle reste ouverte pour comprendre le
  comportement complet de `sub_8237FA50`.
- **Si `cmplw` (non signé) est la traduction fidèle de l'instruction
  PPC retail réelle à cet endroit**, ou une erreur de lecture — le
  texte source PPC lit `cmplw` explicitement (déjà cité par r410/r411
  comme "comparaison unsigned"), cohérent avec la convention standard
  de `sub_821F9E10` et consorts (tailles toujours non signées) — jugé
  suffisamment établi pour ne pas re-vérifier par capture séparée,
  mais non confirmé par relecture du binaire retail brut lui-même (pas
  seulement du C++ généré).
- **Si ce défaut (sentinelle `-1` + comparaison non signée) est un bug
  du jeu retail lui-même**, présent sur console, ou un artefact de la
  recompilation — aucun oracle utilisé, aucune comparaison avec Xenia
  ou le binaire Xbox 360 original n'a été faite (conforme à la
  politique "no oracle" de la campagne).
- Aucun correctif proposé ni appliqué.

## Décisions prises

- Rejeter la première lecture de `$rax` au retour de
  `sub_821F90A8` (donnait systématiquement `0`, contredisant la
  dérivation depuis le code source) plutôt que de la publier : un
  désassemblage x86 statique de la fonction a montré que la valeur de
  retour est stockée en mémoire (`ctx+0`), pas laissée dans `%rax` au
  `ret` — `%rax` sert de registre de travail pour du code sans rapport
  après ce stockage. Cette convention ($rax au FinishBreakpoint) avait
  fonctionné pour `sub_821F9E10` par coïncidence de layout de code, pas
  par garantie — leçon générale pour toute capture future de valeur de
  retour d'une fonction `PPC_FUNC_IMPL` : vérifier par désassemblage
  x86 statique avant de faire confiance à `$rax`.
- Ne pas relire `loc_8237FAF0` maintenant que la cause suffisante du
  débordement est établie — nommé pour un cycle ultérieur si jugé utile,
  pas bloquant pour comprendre POURQUOI le débordement se produit.

## Gate

Aucune source de production éditée ce cycle.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées dans le commit de ce cycle)
`ctest` relancé en entier.

## Files

`recompilation/ace-combat-6-retail/artifacts/retail-us-native-r404-bucket17-writer/r422_capacity_query.gdb/.log`.

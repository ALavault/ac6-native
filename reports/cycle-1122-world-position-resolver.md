# Cycle 1122 — `0x822953F0` lu : où vivent les coordonnées monde

Date : 2026-08-08. La première des deux dettes nommées par le goal.

## Qualification

- Image : Xbox 360 PAL `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- **Corpus de lecture : `ghidra-projects-xenon/ac6-xenon`**, même image importée
  en `PowerPC:BE:64:Xenon`. Ce n'est **pas** le projet canonique et rien n'en est
  fusionné avec lui. Le projet canonique `ghidra-projects/ace-combat-6` n'a pas
  été ouvert en écriture.
- Une écriture a eu lieu **dans le corpus Xenon seul** : quatre adresses
  désassemblées à la demande (`0x82295BB8`, `0x822961CC`, `0x82296260`,
  `0x822962BC`), parce que Ghidra les laissait en données faute d'avoir résolu la
  table de sauts qui y mène. Rien d'autre n'a été touché.
- Charge utile : nœud racine de scénario Mission 01,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- **Statique et produit natif seuls.** Aucun oracle.

## Pourquoi il fallait un second corpus

Dans le projet canonique la fonction s'arrête sur `halt_baddata()` : son calcul
est en VMX128 et le langage chargé ne le décode pas. Le cycle 1115 avait levé le
verrou sans lire la fonction. C'est fait ici.

## Le site d'appel, d'abord

Un balayage des 755 392 instructions décodées du corpus Xenon ne trouve **aucune**
référence à `0x822953F0`, et un balayage de toute la mémoire initialisée n'en
trouve qu'une, dans `.pdata` — c'est-à-dire son propre enregistrement de
fonction, pas un appelant.

L'explication est mesurable : `0x82295B90..0x822964FF`, 2 416 octets, ne sont pas
désassemblés. C'est la table de sauts à dix entrées de `0x82295A88` **et les
corps qu'elle vise**. Désassemblés, le premier parle :

```
82295bd0  stfs f31,0x50(r1)     ; le vecteur de sortie de l'appelant, mis à zéro
82295bd4  or   r5,r30,r30       ; r5 = l'enregistrement de position de l'ordre
82295bdc  addi r4,r1,0x50       ; r4 = le vecteur de sortie
82295be4  or   r3,r31,r31       ; r3 = l'unité agissante — et elle n'est pas lue
82295bf0  bl   0x822953f0
```

L'attribution du cycle 1115 — « `0x82295A88` confie le vecteur à `0x822953F0` » —
tient donc, mais par la branche par défaut de son commutateur, celle que prennent
les sous-genres 0 à 4, 6 et 7.

## La règle

```
82295410  lfs f0,0x8(r31)    \
8229541c  lfs f0,0xc(r31)     >  le triplet, en +0x08 +0x0C +0x10
82295428  lfs f0,0x10(r31)   /
82295414  lbz r10,0x42(r31)  ;  l'octet de mode
82295420  cmplwi cr6,r10,0x1
82295438  bne cr6,0x8229555c ;  différent de 1 : le triplet EST la position
```

Quand le mode vaut 1 :

1. `0x82270380` cherche une unité dans le gestionnaire `global+0x2D3B4` avec les
   octets `+0x43` et `+0x44` ; **absente, la fonction rend 0 et n'écrit rien** ;
2. `0x82093808` prend le cap de cette unité — `0x820936E8`, c'est-à-dire
   `atan2(unité+0x30, unité+0x38)` ;
3. `0x8209CB70` en tire un sinus et un cosinus ;
4. trois `vmsum3fp128` font tourner le triplet autour de l'axe vertical — la
   ligne médiane de la matrice est la constante `(0,1,0)` lue en `0x8204F800`,
   donc **le y ne tourne jamais** ;
5. le résultat s'ajoute à `unité+0x40`, `+0x44`, `+0x48`.

Puis, si le bit 0 du demi-mot `+0x40` est posé, le y est remplacé par ce que rend
une virtuelle de l'objet en `global+0x36084`, **plus** le y de l'enregistrement.

### Le signe, établi et non choisi

Les voies exactes des permutations VMX128 ne sont pas dépliées ici. Le signe l'est
quand même, et par le cap plutôt que par les voies : `0x82093808` calcule
`atan2(avant.x, avant.z)`, donc la direction avant d'une unité de cap `h` est
`(sin h, ·, cos h)`, et un décalage purement avant `(0,0,d)` doit se résoudre en
`d·(sin h, 0, cos h)`. Une seule des deux dispositions le fait. Le test l'exige
au lieu de le commenter.

## Ce que la charge utile en dit — le contrôle

L'octet de mode vient d'une branche. Les deux populations ont été **mesurées
ensuite**, sans être triées à la main :

| | mode 0 (811) | mode 1 (79) |
| --- | ---: | ---: |
| x | −57 600 … 63 000 | −1 800 … 2 000 |
| z | −63 000 … 55 600 | −9 000 … 2 000 |
| médiane (x, y, z) | 2 464 / 700 / −4 696 | **0 / 0 / 0** |

Des coordonnées monde d'un côté, des décalages de l'autre — ce que la branche
exige, et personne ne l'a imposé aux données. **890 enregistrements de position
en Mission 01**, dont 38 portent le drapeau de hauteur.

Un rapprochement, donné et non érigé en règle : 758 des 811 positions absolues
tombent dans le rectangle ±50 000 que la sous-mission 0 installe (cycle 1121).
**53 n'y tombent pas** ; le test compte les 758 sans en tirer d'invariant.

## Ce que cela règle, et ce que cela ne règle pas

**Réglé.** Les coordonnées monde de la Mission 01 existent, sont lisibles, et
sont dans les ordres d'étiquette 2. `include/ac6/retail_world_position.h` et
`src/retail_world_position.cpp` portent le résolveur ; `ctest 24/24`.

**Non réglé — et c'est la moitié qui comptait pour le monde natif.** La position
de **naissance** d'une unité n'est pas un de ces enregistrements. Le sous-record
`Obj` que `build_retail_world` utilise est une autre structure : ses valeurs sont
petites (−50, −6,25, 50 ; 0, −200, −1000), il n'a ni octet de mode ni paire
d'ancre, et **aucun consommateur de ce sous-record n'a été lu**. Sur 230
enregistrements, 122 ont un ordre d'étiquette 2 ; 108 n'en ont aucun.

Il serait facile d'écrire « la naissance est la première position d'ordre » et de
brancher les 122. Ce serait la règle plausible que les cycles 1111 et 1113 ont
appris à tuer : elle n'expliquerait rien des 108 autres et rien ne la contrôle.
Le commentaire de `retail_mission_state.h` dit donc désormais ce que la position
de naissance **n'est pas**, ce qui est le progrès réel de ce cycle.

**Non établi non plus** : ce qu'est l'objet en `global+0x36084` dont la virtuelle
fournit la hauteur — le terrain est l'hypothèse évidente, et elle n'est pas
testée ; la valeur du cap dégénéré `DAT_820542B8` ; les neuf autres sous-genres
de l'ordre d'étiquette 2.

## L'autre dette

Les 1 619 vtables rejetées du balayage RTTI restent intactes.

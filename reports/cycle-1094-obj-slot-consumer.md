# Cycle 1094 — le consommateur du slot `Obj & Unit`

Date : 2026-08-08. G1.1 de l'objectif J1 : qui lit le tampon de scénario.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Charge utile : nœud racine de scénario Mission 01,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- **Statique seul.** Aucun oracle.

## La recherche

Le cycle 1093 a donné l'adresse de la structure analysée. Un balayage du `.text`
pour toute matérialisation `lis 0x125` suivie d'un accès dans
`0x40C0..0x4140` rend **dix sites**, dans cinq fonctions seulement :

| site | forme | champ | fonction |
| --- | --- | --- | --- |
| `0x8219BF14` | `addi` | record+0x00 | `0x8219BDD8` chargeur |
| `0x8219BF44` | `ori` | tampon (base−4) | `0x8219BDD8` |
| **`0x8219C7A4`** | `ori` | **record+0x04 = slot 0** | `0x8219BDD8` |
| `0x8219C9A0` | `ori` | record+0x24 = slot 8 | `0x8219BDD8` |
| `0x8219D078` | `ori` | record+0x10 = slot 3 (`RadioTbl`) | `0x8219BDD8` |
| `0x8219A0C0` | `ori` | record+0x34 | `0x82199F68` |
| `0x8219D860`, `0x821A34BC`, `0x821A378C`, `0x821A3790` | — | tampon, record | démontage |

Les offsets se lisent avec la table de dispatch du cycle 1083 : le champ
`record + 4·(slot+1)`. **`record+0x04` est donc le slot 0, `Obj & Unit`.**

## La chaîne

```
8219c7a0  lis   r10,0x125
8219c7a4  ori   r23,r10,0x40e0     ; r23 = record+0x04, le slot 0 analysé
8219c7ac  lwz   r11,0x1e0(r28)     ; drapeaux du contexte
8219c7b0  rlwinm r11,r11,0,0x1d,0x1d
8219c7b8  beq   cr6, ...           ; garde par bit
8219c7bc  li    r6,0x0             ; sélecteur 0
8219c7c0  lwzx  r4,r28,r23         ; r4 = le pointeur du slot
8219c7cc  bl    0x820A7070
...
8219c7ec  li    r6,0x1             ; sélecteur 1, même slot, autre cible
8219c7fc  bl    0x820A7070
...
8219c9a0  ori   r11,r11,0x4100     ; slot 8
8219c9ac  bl    0x820A7070         ; sélecteur 2
```

`0x820A7070` est donc un consommateur générique, appelé **trois fois** avec des
sélecteurs 0, 1, 2, chacun sous la garde d'un bit distinct de `contexte+0x1E0`.

## Ce que le consommateur fait du slot

```c
if (*(char *)*(u32 *)param_2 != '\0') {        // compteur du conteneur analysé
  do {
    piVar22 = (int *)(i * 0xc + *(int *)(param_2 + 4));   // élément, foulée 0x0C
    cVar1  = *(char *)(*piVar22 + 0x0d);                  // octet de type
    switch (*(u8 *)(*piVar22 + 0x08)) { case 0..4: ... }  // classement 5 voies
    ...
    switch (*(u8 *)(*(int *)(cVar1 * 8 + *(int *)(iVar13 + 4)) + 0x2c)) { ... }
    ...
    *(int *)(kind * 0x44 + *(int *)(contexte + 0x58) + 0x24) += 1;   // compteur
    *(uint *)(iVar13 + 0x60) |= 0x4000 | 0x4200 | 0x4400 | 0x5800 | 0x1000;
  } while (i < *(u8 *)*(u32 *)param_2);
}
```

Trois points, tous vérifiables dans le listing :

1. **La borne de boucle est le compteur analysé.** `*(u8*)(*(u32*)slot)` est
   exactement le mot `data` du conteneur et son premier octet, la primitive
   dérivée au cycle 1084. Le consommateur itère la structure produite par le
   parseur, pas une structure parallèle.
2. **La foulée est `0x0C`**, celle des enregistrements de `0x8232CCA0` — le
   niveau immédiatement sous le slot 0.
3. **Le résultat est une écriture** : un compteur par type incrémenté dans une
   table de foulée `0x44` basée en `contexte+0x58`, et des bits d'activation
   posés en `+0x60` sur l'objet cible.

## Prédiction, et vérification sur les octets

Le `switch` de la ligne 100 n'implémente que les valeurs **0 à 4** ; toute autre
valeur laisse `lVar19 = 0` et `iVar18 = 0`. C'est une prédiction sur les données.

Relevé sur les **230** entrées de niveau 0 de la Mission 01 :

| `data+0x08` | 0 | 1 | 2 | 3 | 4 | > 4 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| occurrences | 1 | 40 | 188 | 0 | 1 | **0** |

**Aucun enregistrement ne sort du domaine que le consommateur implémente**, sur
230 essais, avec 0 anomalie de parcours. Le champ `data+0x0D`, utilisé comme
index dans une table de foulée 8, ne prend que trois valeurs (`0` → 140,
`1` → 42, `2` → 48), cohérent avec un petit index de type.

C'est la même forme de preuve que les validations d'étiquette des cycles 1084 à
1090, mais du côté **consommateur** : elle relie des octets analysés à un
comportement d'exécution.

## Ce que cela établit

La sortie que G1 exigeait : pour deux champs d'un enregistrement analysé
(`data+0x08`, `data+0x0D`), une chaîne propriétaire → consommateur complète, où
la valeur **devient** un classement, une comparaison contre un registre de
16 entrées (`PTR_DAT_826E4EB4 + 0x11C`, foulée 60), un compteur incrémenté et
des bits d'activation.

Ce n'est pas une adjacence d'étiquette. La borne de boucle *est* le compteur
analysé.

## Ce que cela n'établit pas

- **`H-STATIC-OBJ-AND-UNIT-REGION-IS-THE-PARSED-BUFFER` reste `proposed`.** La
  chaîne suivie ici part de `record+0x04`, pas de `contexte+0x2E8`. Les deux
  restent des régions distinctes ; rien dans ce cycle ne les rapproche.
- `0x820A7070` n'atteint ni la factory `0x820A7F48`, ni `0x822707C8`, ni
  `0x8226D1C8` en moins de 6 sauts. Ce n'est donc pas le créateur d'unités.
- Ce que signifient les cinq classes de `data+0x08`, ni ce que compte le
  compteur `+0x24`, ni ce qu'activent les bits `+0x60`.
- Aucune identité de vague, aucune condition d'objectif.
  `retail_units_and_waves` et `retail_objectives` restent **ouverts**.

## Correction — ce que « la cible » désigne

La première rédaction de ce cycle parlait d'une « méthode virtuelle de la cible »
et de « bits d'activation sur l'objet cible ». **C'était attribué à un objet non
établi.** Le décompilé rend `piVar10 = __savegprlr_14()`, ce que Ghidra modélise
comme la valeur de retour de l'aide de prologue, donc comme `r3`. L'assembleur
dit autre chose :

```
820a7088  or r28,r4,r4      ; r28 = le pointeur du slot analysé
820a7090  or r25,r3,r3      ; r25 = le premier argument
820a70d8  lis r18,-0x7d92
820a70e4  ori r19,r11,0x9c80
820a7334  lwzx r3,r11,r19   ; r3 = *(global + 0x29C80) = le contexte de mission
820a7338  lwz  r11,0x0(r3)
820a733c  lwz  r11,0x4(r11)
820a7344  bctrl             ; appel virtuel sur le CONTEXTE, pas sur param_1
```

Les appels virtuels portent donc sur le **contexte de mission**, et les
écritures visent des objets dérivés de lui — `type*0x44 + *(contexte+0x58)` pour
le compteur, `*(*(contexte+0x264) + 0x18) + 0x60` pour les bits. Rien de tout
cela n'est « la cible » passée en argument.

Et le premier argument n'est pas un objet du tout. Il vient de `0x821B7300`,
qui écrit trois sorties depuis des tables indexées par le niveau
(`DAT_820658D8`, `DAT_82065920`, `DAT_820659A0`) et rend des petits entiers —
`0x86`, `0x87`, `0x92`, `0x96`, `0xA1`, `0xFFFFFFFF`. **C'est un identifiant,
pas un pointeur.**

Ce qui reste intact : la borne de boucle est le compteur analysé, la foulée est
`0x0C`, les champs `data+0x08` et `data+0x0D` sont lus par enregistrement, et
les 230 enregistrements tombent dans le domaine du `switch`. Ces énoncés
dépendent de `param_2`, qui est sans ambiguïté (`r4` → `r28`).

## Précision sur la valeur classée

Le `switch` de `data+0x08` produit une valeur — `lVar19` — qui vaut 1, 4, 4, 4, 3
selon la classe, puis 2, 5 ou 6 selon les branches ultérieures. La lecture
tentante est « taille de formation » : un appareil seul, une patrouille de
quatre, une de trois.

**Elle est fausse.** La valeur est passée telle quelle à une méthode virtuelle de
la cible :

```c
iVar13 = (**(code **)(*piVar10 + 0x10))(piVar10, lVar19, iVar20 == 0);
```

et sert ensuite de discriminant à trois comparaisons qui choisissent les bits
d'activation (`== 4` → `0x5800`, `== 5` → `0x4200`, `== 6` → `0x4400`). C'est
donc une **catégorie** d'un petit domaine (1 à 6), pas un compte.

La catégorie est passée à un appel virtuel — mais sur le contexte de mission,
comme le montre la correction ci-dessus, pas sur un objet cible. Le compteur
`+0x24` et les bits `+0x60` sont des traces d'activation ; **quel objet les
reçoit reste à établir.**

## Prochaine tranche

1. Qualifier le slot virtuel appelé sur le contexte de mission et le domaine de
   catégorie 1 à 6 qu'il reçoit, ainsi que l'identifiant issu de `0x821B7300`
   (tables `DAT_820658D8`, `DAT_82065920`, `DAT_820659A0`, indexées par niveau).
2. Qualifier la table `contexte+0x58` de foulée `0x44` et son champ `+0x24` :
   un compteur par type d'objet est le candidat le plus direct pour
   `retail_units_and_waves`.
3. G1.2 reste ouverte : ce que `0x8226EBD0` fait de `contexte+0x2E8`.

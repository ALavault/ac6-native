# Cycle 1093 — où la structure de scénario atterrit dans le runtime

Date : 2026-08-08. Premier pas hors de la structure, vers la sémantique.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- **Statique seul.** Aucun émulateur, aucun bridge, aucune exécution du produit
  natif.

## La chaîne, instruction par instruction

Le chargeur de mission `0x8219BDD8` **se publie lui-même** dès son prologue :

```
8219bdf0  or    r28,r3,r3          ; r28 = le contexte de mission
8219be00  lis   r11,0x2
8219be04  ori   r10,r11,0x9c80     ; 0x29C80
8219be08  lwz   r11,0x4eb4(r21)    ; PTR_DAT_826E4EB4
8219be0c  stwx  r28,r11,r10        ; *(global + 0x29C80) = r28
```

Puis il dimensionne, alloue et remplit la structure de scénario :

```
8219bf04  bl    0x82234DD0         ; enfant 0 de la ressource DPL décodée
8219bf10  or    r30,r3,r3          ; r30 = le noeud racine de scénario
8219bf08  addis r31,r28,0x125
8219bf14  addi  r31,r31,0x40dc     ; r31 = contexte + 0x12540DC   <- l'enregistrement
8219bf24  bl    0x822493F0         ; getReadBuffSize(this=r31, node=r30)
8219bf38  bl    0x82222E98         ; alloue ce nombre d'octets
8219bf44  ori   r11,r11,0x40d8
8219bf50  stwx  r5,r28,r11         ; *(contexte + 0x12540D8) = le tampon
8219bf54  bl    0x82249718         ; read(this=r31, node=r30, buffer)
8219bf60  ...   'Mission Data'     ; étiquette d'allocation
```

`0x82234DD0` est appelé avec **index d'enfant 0**, ce qui confirme par une
troisième voie l'identification du cycle 1083 : le nœud racine de scénario est
l'enfant 0 de l'entrée 9.

## Ce que cela établit

**La structure de scénario a une adresse exacte dans le contexte de mission :**

| adresse | contenu |
| --- | --- |
| `global + 0x29C80` | le contexte de mission lui-même, publié par le chargeur |
| `contexte + 0x12540DC` | l'**enregistrement racine de scénario**, celui que `0x82249718` remplit |
| `contexte + 0x12540D8` | le **tampon** où atterrissent tous les enregistrements `ObjBin`, `OrderBin`, `ActBin`, `SetBin`, `ManeuverBin`, `ComTblBin`, `ComBin` |

Autrement dit : les 138 enregistrements micro-exécutés aux cycles 1089 à 1092
ne vivent plus dans un espace synthétique. On sait **où** le jeu les range.

C'est l'ancrage qui manquait pour chercher leurs consommateurs : il suffit
désormais de chercher qui lit `contexte + 0x12540D8`, au lieu de chercher une
structure sans adresse.

## Une piste voisine, et pourquoi elle ne se referme pas

Le même chargeur, juste après avoir émis l'étiquette d'allocation
`'Obj & Unit'` (`0x8219C974`), écrit :

```
8219c97c  addis r5,r28,0x12
8219c984  addi  r5,r5,0x3c40       ; r5 = contexte + 0x123C40
8219c990  stw   r5,0x2e8(r28)      ; contexte->0x2E8 = cette région
```

Et `0x8226D1C8` — `mission_manager_update`, **déjà qualifiée** dans
`analysis/address_catalog.tsv` — la relit :

```
8226d794  lwz   r11,0x260(r28)
8226d798  cmpwi cr6,r11,0x8        ; seulement si état == 8
8226d7ac  lwzx  r11,r11,r10        ; *(global + 0x29C80)
8226d7b0  lwz   r31,0x2e8(r11)
8226d7b8  beq   cr6,...            ; ignoré si nul
8226d7e8  or    r3,r31,r31
8226d7ec  fmr   f1,f30             ; un flottant, vraisemblablement le delta
8226d7f0  bl    0x8226EBD0
```

C'est un lien propriétaire → consommateur complet, sous garde d'état, entre le
chargeur et une fonction de mise à jour qualifiée.

**Mais `0x123C40` n'est pas `0x12540DC`.** La région publiée en `+0x2E8` est
*distincte* du tampon de scénario. L'étiquette `'Obj & Unit'` la précède
immédiatement, ce qui est suggestif — et rien de plus. Rapprocher les deux
serait exactement le raccourci que le cycle 1073 a payé cher.

Ce cycle **n'affirme donc pas** que `contexte+0x2E8` porte les enregistrements
`Obj` analysés. Il constate deux régions voisines, l'une nommée par une
étiquette d'allocation, l'autre remplie par le parseur, et il donne l'adresse
des deux.

## Ce que cela n'établit pas

- Que `contexte + 0x2E8` contienne les enregistrements `ObjBin` analysés.
- Ce que `0x8226EBD0` fait de la région qu'il reçoit.
- Une identité de vague, une condition d'objectif, ou une insertion dans
  `UnitManager` — ce que réclame le discriminateur de
  `H-RETAIL-OBJECTIVE-WAVE-OWNER-STATIC-BOUNDARY`.

`retail_units_and_waves` et `retail_objectives` restent **ouverts**.

## Prochaine tranche, précise

Deux questions, dans cet ordre, toutes deux statiques :

1. **Qui lit `contexte + 0x12540D8`** — le tampon de scénario ? C'est la
   question directe, et elle a maintenant une adresse à chercher
   (`addis`/`lwz` avec partie haute `0x125` et déplacement `0x40D8`).
   Ses lecteurs sont, par construction, les consommateurs des enregistrements
   dont les cycles 1089–1092 ont prouvé la disposition.
2. **Que fait `0x8226EBD0`** de `contexte + 0x2E8` ? Répondre dit si la région
   `Obj & Unit` est un registre d'unités ou autre chose.

Ne pas rejouer : la localisation du nœud racine de scénario est close par trois
voies indépendantes (cycles 1083, 1092, et l'index d'enfant 0 de `0x82234DD0`
ici). Ne pas engager de passage N3 tant que la question 1 n'a pas été posée en
statique.

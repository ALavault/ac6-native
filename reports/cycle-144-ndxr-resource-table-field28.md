# AC6 — contrat statique de la table de ressources du champ `+0x28`

Date : 2026-07-17 (Europe/Paris)

## Cible et méthode

Cible : `default.xex` Xbox 360 PAL, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

Lecture headless et non destructive de `0x82234bd8..0x82234ee0`, appelée par
la famille de méthodes du sous-objet `0x8205c980`. Aucun code généré, export ou
runtime n'est modifié.

## Helpers de lookup borné

Le helper `0x82234dd0` exécute le contrat local suivant :

```text
if (u32be(r3+0x00) <= r4) return 0;
entry = u32be((u8*)r3+0x0c + 4*r4);
if (entry == 0) return 0;
return u32be(r3+0x04) + entry;
```

Le helper `0x82234e08` est une variante qui lit une autre table :

```text
if (u32be(r3+0x00) <= r4) return 0;
return u32be((u8*)r3+0x10 + 4*r4);
```

Les deux vérifient donc la borne par rapport au compteur à `r3+0x00` avant de
lire l'entrée indexée. Les noms restent neutres :
`bounded_relative_table_lookup` et `bounded_pointer_table_lookup`.

## Producteur du champ `+0x28`

Dans `0x820fa9c0` :

```text
... construction d'un descripteur local à la pile ...
li   r4,0xb
addi r3,r1,0x50
bl   0x82234e08
or   r11,r3,r3
stw  r11,0x28(r31)
```

Une branche ultérieure peut remettre `+0x28` à zéro lorsque le champ
`+0x5c` vaut `4`. Le fait confirmé est donc :

```text
subobject+0x28 = pointer_table_entry(local_descriptor, index=0xb)
```

Le worker lit ensuite ce champ comme base de sa boucle et charge des mots dans
la table (`lwz r31,0(r28)` après un offset calculé). Cela rend sûre la
qualification `resource_pointer_table` pour le rôle structurel de `+0x28`, sans
nommer encore son contenu NDXR, scène ou renderer.

## Relation avec le problème `r4`

Le nouveau contrat explique pourquoi le worker parcourt une table de mots, mais
ne résout pas la préparation :

```text
r31 = table_entry
r4  = (r31 >> 16) & 0x1ff
```

La feuille candidate `0x82101be0` traite ensuite `r4` comme une adresse pour
`lhz r4+0x1c`. Il faut encore déterminer si :

- l'instance dynamique utilise un override de vtable au slot `+0x5c` ;
- le mot de table est un encodage dont le champ 9 bits est un alias adressable ;
- une étape de registre/entrée a été mal classée par la frontière Ghidra.

Ne pas implémenter de pointeur ou de record sur cette base seule.

## Niveau de confiance

`confirmed` :

- bornes et formes des deux helpers de lookup ;
- index littéral `0xb` utilisé pour initialiser `+0x28` ;
- lecture de `+0x28` comme base de parcours dans le worker.

`unknown` :

- contenu et provenance runtime des entrées de la table ;
- cible effective du slot `+0x5c` pour chaque instance ;
- signification du champ 9 bits et du mot retourné dans `r5`.

## Validation

```bash
git diff --check
ctest --test-dir .build/ace-combat-6/native --output-on-failure
```

La suite native reste à **41/41 PASS** ; aucune modification de code natif n'a
été nécessaire.

## Suite sans action humaine

Suivre les writers du descripteur local et les éventuels overrides du vtable
après la construction. Une session runtime ne sera envisagée qu'après
épuisement de ces corrélations statiques.


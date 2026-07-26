# AC6 — contrat statique de la table indexée du worker NDXR (cycle 153)

## Cible et méthode

- target : `ac6-xbox360-pal-default-xex` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- projet : `ghidra-projects/ace-combat-6` ;
- mode : `analyzeHeadless -readOnly -noanalysis` ;
- scripts : `FindInstructionScalar.java` et `DumpRange.java` ;
- aucune écriture dans le XEX, le projet Ghidra, les exports ou le runtime.

Cette passe suit uniquement les champs consommés par la feuille `0x82100600`.
Elle cherche un contrat de table vérifiable, sans donner de nom de gameplay à
un handle ou à un pointeur de ressource.

## Résultats

### Le champ `owner+0x74` borne le nombre d'entrées

Le même corps de préparation qui alimente `owner+0x28` initialise et met à
jour `owner+0x74` :

```text
0x820fb038  stw r26,0x78(r31)
0x820fb040  stw r26,0x74(r31)    ; remise à zéro
...
0x820fb0ac  lwz r11,0x74(r31)
0x820fb0b4  addi r11,r11,0x1
0x820fb0bc  stw r11,0x74(r31)    ; progression bornée
```

La boucle environnante parcourt `r30 = 0..0xff` et n'incrémente le champ qu'au
terme d'une résolution non nulle. Le champ est donc confirmé comme une limite
ou cardinalité d'une famille de 256 entrées maximum pour ce chemin. Son rôle
exact (nombre de ressources valides, slots actifs ou autre compteur de
préparation) reste volontairement non nommé.

Les lecteurs du worker et de la feuille sont cohérents :

```text
worker    : lwz r10,0x74(r18) via la préparation et le dispatch
0x82100600: lwz r11,0x74(r3), puis compare r4 à r11
```

### La base de la table est `owner+0x6d8c`

La feuille voisine `0x821005e8` effectue le test de présence et renvoie la base
de la table :

```text
0x821005e8  lwz    r11,0x74(r3)
0x821005ec  addi   r3,r3,0x6d8c
0x821005f0  cmplwi cr6,r11,0x0
0x821005f4  bnelr  cr6
0x821005f8  li     r3,0x0
0x821005fc  blr
```

Cette entrée est à `+0x58` de l'address-point `0x8205c9a4` (`0x8205c9fc`),
juste avant le slot worker `+0x5c`. Elle fournit une paire statique cohérente
avec le dispatch :

- `+0x58` : obtenir la base `owner+0x6d8c` si la famille est non vide ;
- `+0x5c` : obtenir une entrée bornée de cette base.

### La feuille `0x82100600` est un accessor indexé

La cible du slot worker est :

```text
0x82100600  lwz    r11,0x74(r3)
0x82100604  cmplw  cr6,r4,r11
0x82100608  bge    cr6,0x8210061c
0x8210060c  addi   r11,r4,0x1b63
0x82100610  rlwinm r11,r11,0x2,0x0,0x1d
0x82100614  lwzx   r3,r11,r3
0x82100618  blr
0x8210061c  li     r3,0x0
0x82100620  blr
```

Le calcul `((r4 + 0x1b63) << 2)` équivaut à
`owner + 0x6d8c + r4*4` dans la plage bornée par `owner+0x74`. Le contrat
statique est donc :

```text
r3 : owner
r4 : index numérique [0, owner+0x74)
ret: owner[0x6d8c + r4*4], ou zéro hors limite
```

Il s'agit d'un accessor de table confirmé par le code. La nature de chaque
élément retourné (handle, pointeur de ressource ou autre valeur 32 bits) reste
`unknown` tant qu'un consommateur ou une trace ne l'établit pas.

## Décision et portée

- Le worker doit conserver `hi9` comme index borné et appeler le slot
  `0x82100600`.
- L'adaptateur natif peut exposer une primitive interne
  `get_indexed_entry(owner, hi9)` avec retour nullable, sans publier une
  sémantique NDXR ou renderer.
- `owner+0x74` doit être traité comme limite de capacité/validité, pas comme
  une adresse ou un pointeur.
- `owner+0x6d8c` est une base de table distincte de `owner+0x28` (descripteurs)
  et de `owner+0x5c` (champ mémoire de préparation).
- La cible `0x82101be0` reste exclue de ce chemin : elle se trouve à un autre
  slot (`+0x38`) et possède un contrat de registre incompatible.

Ces invariants sont `confirmed` statiquement. L'identité sémantique des valeurs
retournées et l'état concret d'un owner en exécution restent `cross-match` tant
qu'une validation différée ne les observe pas. Aucune action humaine, session
Xenia ou capture manuelle n'est nécessaire pour cette passe.

## Validation documentaire

- `FindInstructionScalar.java 0x74` : writers/readers du champ relevés ;
- `FindInstructionScalar.java 0x6d8c` : base de table confirmée dans la feuille
  voisine ;
- `DumpRange.java` sur `0x820fb038..0x820fb224` et `0x821005e8..0x82100620` :
  PASS ;
- aucun fichier généré, binaire ou projet Ghidra modifié : PASS.

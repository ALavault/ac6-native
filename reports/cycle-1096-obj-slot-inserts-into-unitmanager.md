# Cycle 1096 — l'enregistrement `Obj & Unit` entre dans `CX360UnitManager`

Date : 2026-08-08. G1 de l'objectif J1, et la fermeture du discriminateur de
`H-RETAIL-OBJECTIVE-WAVE-OWNER-STATIC-BOUNDARY` — **en statique**.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Charge utile : nœud racine de scénario Mission 01,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- **Statique seul.** Aucun émulateur, aucun bridge, aucun passage N3.

## Ce que le cycle 1094 avait mal attribué

Le cycle 1094 affirmait que le premier argument de `0x820A7070` n'était « pas
un objet » mais un identifiant issu de `0x821B7300`, et que les appels virtuels
portaient sur le contexte de mission. **C'est faux, et c'est corrigé ici.**

`0x821B7300` est appelé par le *chargeur* `0x8219BDD8` (sites `0x8219C8xx` et
`0x8219C9xx` de la boucle de ressources), pas sur le chemin d'argument de
`0x820A7070`. L'assembleur des trois sites d'appel le dit sans ambiguïté :

```
8219c748  addis r22,r28,0x13     ; r22 = contexte + 0x12B440
8219c760  subi  r22,r22,0x4bc0
8219c7c8  or    r3,r22,r22       ; premier argument = cet objet
8219c7cc  bl    0x820A7070
```

et le prologue du consommateur appelle bien une méthode virtuelle **de cet
objet** :

```
820a7090  or   r25,r3,r3
820a70b4  lwz  r11,0x0(r25)      ; vtable de param_1
820a70c0  lwz  r11,0xc(r11)      ; slot +0x0C
820a70d4  bctrl
```

Ghidra rend `param_1` comme `piVar10 = __savegprlr_14()` — un artefact de
l'aide de prologue, pas une valeur de retour. Le cycle 1094 a pris l'artefact
pour la chose. La leçon vaut d'être écrite : *quand le décompilé fait sortir un
pointeur d'une aide de prologue, il faut lire les registres.*

## Les cinq objets nommés du contexte de mission

Le destructeur du contexte `0x8219B948` installe les tables virtuelles avant
d'appeler chaque destructeur de classe. Cinq objets embarqués apparaissent,
et la RTTI MSVC les nomme :

| adresse | vtable installée | classe (RTTI) | taille |
| --- | --- | --- | ---: |
| contexte + `0x11D470` | `0x8205516C` | `CX360ObjManager` : `ACE6::CAce6ObjManager` : `ACE6::CAce6Thread` | `0x33E8` |
| contexte + `0x120858` | `0x8205516C` | idem | `0x33E8` |
| contexte + `0x123C40` | `0x8205516C` | idem | `0x33E8` |
| contexte + `0x12B440` | `0x82055190` | `CX360UnitManager` : `ACE6::CAce6UnitManager` : `ACE6::CAce6Thread` | `0x41C` |
| contexte + `0x12B85C` | `0x82055190` | idem | `0x41C` |

Les tailles se lisent aux écarts d'adresses (`0x120858 − 0x11D470 = 0x33E8`,
`0x12B85C − 0x12B440 = 0x41C`) et concordent avec ce que les constructeurs
mettent à zéro. **Le troisième `CX360ObjManager` est celui que le chargeur
publie en `contexte + 0x2E8`** — la région dont le cycle 1095 a montré qu'elle
est parcourue chaque trame en 32 tranches.

## Les trois appels, avec leurs arguments exacts

Le décompilé du chargeur donne la forme canonique :

```c
Function_820A7070(contexte+0x12B440, *(contexte+0x12540E0), contexte+0x11D470, 0);
Function_820A7070(contexte+0x12B85C, *(contexte+0x12540E0), contexte+0x120858, 1);
Function_820A7070(contexte+0x12B440, *(contexte+0x1254100), contexte+0x123C40, 2);
```

soit `(gestionnaire d'unités, slot analysé, gestionnaire d'objets, sélecteur)`.
`0x12540E0` est `enregistrement+0x04` = slot 0 `Obj & Unit` ; `0x1254100` est
`enregistrement+0x24` = slot 8. Chaque appel est gardé par un bit distinct de
`contexte+0x1E0`.

## Ce que le consommateur fait de chaque enregistrement

Par élément (foulée `0x0C`, borne = l'octet de compte analysé) :

```c
lVar19 = classe(data[0x08]);                       // 0→1, 1→4, 2→4, 3→4, 4→3
iVar18 = drapeaux(faction(data[0x0D]).octet 0x2C); // 0x20000000 / 0x40000000 / 0x80000000
objet  = (**(vtable(gestionnaire) + 0x10))(gestionnaire, lVar19, sélecteur == 0);
objet[0xD0] = index de l'élément;
objet[0xD4] = iVar18;
Function_8226FEC0(gestionnaire, objet);            // insertion
...
objet[0xD8] = &gestionnaireObjets[compte + 2];     // = région + 8 + compte*4
objet[0xDC] = nombre d'Obj de l'enregistrement;
objet[0xE0] = enregistrement Set analysé;
objet[0xE4] = pointeur data de l'enregistrement analysé;
```

`0x820A7F48`, le slot virtuel `+0x10`, est **la factory que le cycle 1073 avait
isolée sans pouvoir la joindre à des données**. Son corps est un `switch` sur
`catégorie − 1` : les cas 0 et 1 allouent `0x100` octets, le cas 2 `0x230`, les
cas 3 à 5 délèguent à `0x820A8E08`. La catégorie n'est donc ni un compte ni une
taille de formation : **elle choisit la classe et la taille de l'objet créé.**

## L'insertion, littéralement

`0x8226FEC0` tient en dix lignes et ne laisse aucune place à l'interprétation :

```c
void Function_8226FEC0(int gestionnaire, int *objet) {
  if ((**(code **)(*objet + 4))(objet))        *(int **)(gestionnaire + 0x404) = objet;
  else if ((**(code **)(*objet + 8))(objet))   *(int **)(gestionnaire + 0x408) = objet;
  *(int **)((*(int *)(gestionnaire + 0x40c) + 1) * 4 + gestionnaire) = objet;
  *(int *)(gestionnaire + 0x40c) = *(int *)(gestionnaire + 0x40c) + 1;
}
```

Le tableau visé commence en `gestionnaire + 0x04` et le compteur est en
`+0x40C`. C'est exactement la table que le constructeur de base décrit dans
`ENTRY9_X360_UNIT_MANAGER_REPORT.md` met à zéro : **256 emplacements de
`+0x04` à `+0x400`**, puis `+0x404`, `+0x408`, `+0x40C`.

Autrement dit : **chaque enregistrement analysé produit un objet qui est
inséré, à un index atomiquement incrémenté, dans la table de 256 emplacements
d'un `CX360UnitManager`.** C'est le libellé du discriminateur de
`H-RETAIL-OBJECTIVE-WAVE-OWNER-STATIC-BOUNDARY`, obtenu sans oracle.

## La table de factions, et le recensement par faction

`data[0x0D]` n'est pas un index libre. Le consommateur le passe dans

```c
faction = *(int *)(*(int *)(enregistrement + 0x18) + 4) + data[0x0D] * 8;
switch (*(u8 *)(*(int *)faction + 0x2C)) { ... }   // 9 voies
```

`enregistrement + 0x18` est le **slot 5** de la racine de scénario, et le
chargeur y lit le même compte pour dimensionner un tableau :

```c
uVar23 = *(byte *)*(*(contexte+0x264) + 0x18);         // 4 factions
piVar9[0x16] = alloue(uVar23 * 0x44);                   // contexte + 0x58
// chaque entrée : +0x08 = 0xFF, +0x24 = 0
```

et `0x820A7070` termine chaque itération par
`*(kind*0x44 + *(contexte+0x58) + 0x24) += 1`. **`contexte+0x58` est donc un
recensement par faction, dimensionné par la table de factions elle-même.**

Relevé sur la charge utile : le slot 5 déclare **4** factions ; les 230
enregistrements n'en utilisent que trois — `0` × 140, `1` × 42, `2` × 48 — et
les quatre entrées portent `octet 0x2C = 0`, donc la voie 0 du `switch` et le
drapeau `0x20000000` pour tous.

## La liste `Obj` sous chaque enregistrement

`enregistrement[2]` pointe une liste analysée par `0x8232F380`, qui écrit deux
tableaux parallèles : des éléments de 8 octets (`0x8232F198`) et des
enregistrements de **`0x20` octets remplis par `ObjBin::read` `0x82330158`**.
Le consommateur parcourt le second, foulée `0x20`, et pour chaque élément :

```
820a770c  lwz  r28,0x0(r26)      ; mot 0 de l'enregistrement ObjBin = son pointeur data
820a777c  lbz  r5,0x56(r28)      ; l'octet passé à la factory
820a7784  lwz  r11,0x14(r11)     ; slot virtuel +0x14 = 0x820A8138
820a778c  bctrl
```

`0x820A8138` construit une table de 15 triplets `{clé, constructeur, code}`
avec les clés 0 à 14, cherche la clé égale à l'octet reçu, appelle le
constructeur correspondant et écrit le `code` en `objet+0xB8`.

Mesure sur la charge utile : **434 enregistrements `ObjBin`**, tous avec
`data+0x56 = 0`, donc tous la clé 0. Le champ est lu, son domaine est celui de
la table, et Mission 01 n'en exerce qu'une valeur. C'est un fait, pas une
identité de modèle.

## L'ordre de peuplement, vérifié dans le chargeur

Entre le deuxième et le troisième appel, le chargeur relit le premier
`CX360ObjManager` :

```
8219c854  subi r25,r28,0x2b8c    ; contexte + 0x11D474  = région + 0x04, le compte
8219c864  lwz  r11,0x0(r25)
8219c86c  ble  cr6,0x8219c96c    ; rien à faire si le compte est nul
8219c874  subi r23,r28,0x2b88    ; contexte + 0x11D478  = région + 0x08, le tableau
8219c878  lwz  r29,0x0(r23)      ; un objet construit
8219c87c  lwz  r26,0x15c(r29)    ; sa ressource MDLP
8219c880  lwz  r27,0xb4(r29)
```

Le chargeur parcourt donc, immédiatement après les deux premiers appels, le
même `CX360ObjManager` que ces appels ont reçu en troisième argument, et lit
sur ses éléments les champs `+0x15C` et `+0xB4` de l'objet construit.

**Précision à ne pas laisser filer** : `0x820A7070` *lit* le compte
`région+0x04` pour calculer `objet+0xD8 = région + 8 + compte*4`, mais on ne
voit dans cette fonction **aucune écriture** sur `région+0x04` ni sur le
tableau `région+0x08`. Le seul incrément atomique établi ici est celui du
`CX360UnitManager` (`+0x40C`, par `0x8226FEC0`). Qui fait avancer le compte du
`CX360ObjManager` reste à trouver ; ce cycle ne l'affirme pas.

## Ce que cela établit

Pour deux champs d'un enregistrement analysé, une chaîne complète et vérifiable
instruction par instruction :

- `data[0x08]` → catégorie → **classe et taille de l'objet construit** par
  `0x820A7F48` ;
- `data[0x0D]` → entrée de la table de factions → **drapeaux de camp** et
  **incrément du recensement par faction** en `contexte+0x58` ;
- l'objet construit est **inséré** dans la table de 256 emplacements du
  `CX360UnitManager` par `0x8226FEC0`, et conserve en `+0xE4` le pointeur vers
  l'enregistrement analysé qui l'a produit.

`H-RETAIL-OBJECTIVE-WAVE-OWNER-STATIC-BOUNDARY` est **fermée en statique**.
Le passage N3 pré-enregistré au cycle 1095 devient sans objet : son
discriminateur est satisfait par le code, et un run ne pourrait que le
confirmer plus faiblement.

## Ce que cela n'établit pas

- **Aucune identité de modèle.** Ni `data+0x56`, ni la catégorie, ni l'index de
  faction ne nomment un appareil. Les dictionnaires de noms de l'entrée 9 ont
  des identifiants opaques (`v069_1099`) ; aucun champ de l'arbre analysé n'a
  été joint à eux, et le cycle 1094 avait déjà rejeté la version bon marché de
  cette idée.
- **Aucune durabilité, aucune vitesse, aucun rayon.** Ces champs n'existent pas
  dans l'enregistrement analysé.
- Ce que signifient les codes de la table à 15 entrées de `0x820A8138`, et ce
  que fait le second gestionnaire d'unités (`0x12B85C`) de son propre passage.
- La refonte de catégorie par la table de factions n'a lieu que si le mode de
  jeu vaut 2 ou 3 ; c'est un état d'exécution, non tranché ici.

## Correction portée au dossier

`H-STATIC-OBJ-AND-UNIT-REGION-IS-THE-PARSED-BUFFER` est **rejetée** dans sa
forme littérale : `contexte+0x2E8` n'est pas le tampon analysé, c'est le
troisième `CX360ObjManager`, celui que le troisième appel remplit depuis le
slot 8. Les deux régions restent distinctes — et on sait maintenant ce qu'est
la seconde.

# AC6 cycle 242 — fixup MATE et propriétaire RTTI du type `0x0C`

## Cible et portée

- target : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- projet Ghidra canonique : `ace-combat-6`.

Cette passe qualifie le résultat utile obtenu après l'archive
`ac6_material_bind_xex_boundary_v1.zip`. Elle utilise Ghidra headless en
lecture seule. Aucun Xenia, Wine, VNC, GUI, writer Ghidra ou asset retail n'a
été modifié.

> Erratum qualifié au cycle 243 : l'appel virtuel de `0x8233EE40` lit le mot de
> vtable au décalage byte `+0x04`, pas `+0x08`. Il s'agit de l'incrément de
> référence de `NU::Texture::TextureContextConcrete`; le décalage `+0x08`
> contient la libération correspondante. Le reste du contrat de fixation MATE
> de ce rapport demeure inchangé.

## Décision de non-duplication

Les cycles 137, 191 et 192 avaient déjà observé `0x823330F0` comme une
normalisation de pointeurs et le service global à vtable `0x820674D8`. Ils ne
connaissaient toutefois ni le format MATE, ni la routine inverse, ni le type
C++ concret du service.

Cette passe conserve donc leurs contrats bruts et clarifie uniquement :

1. `0x823330F0` est le **défixeur MATE** ;
2. son inverse exact est `0x82332E48` ;
3. le service est `CX360ActorModelSetup`, dérivé de
   `ACE6::CActorModelSetup` ;
4. son handler associe les ressources de type `0x0C` à un objet propriétaire
   au champ `+0x30`.

Les anciennes attributions avion, caméra ou draw restent exclues : le nom de
classe qualifie un setup de modèle, pas son consommateur final.

## Couple inverse MATE

### Fixup `0x82332E48`

Le thunk de sauvegarde commence à `0x82332E48`; le corps est
`0x82332E50..0x823330E8`. ABI observée :

```text
r3 = base du bloc MATE
r4 = mode, seul l'octet faible est utilisé
r3 au retour = booléen de succès
```

Sur une représentation encore relative, le corps :

- écrit `1` à `base+0x20` ;
- ajoute `base` aux trois offsets `+0x0C`, `+0x10` et `+0x14` ;
- parcourt `u16(base+0x04)` entrées de table, stride `0x10`, et ajoute `base`
  au premier mot de chacune ;
- parcourt pour chaque matériau `u16(material+0x0A)` sous-records depuis
  `material+0x20`, stride `0x18` ;
- résout le premier mot de chaque sous-record via `0x8233EE40`, le registre
  global `0x828C8100` et `0x8233EBB0`, puis appelle le slot virtuel byte `+0x04` de
  l'objet résolu ;
- fixe ensuite la liste chaînée située après les sous-records en convertissant
  ses offsets relatifs en pointeurs.

Lorsque le bit 0 était déjà posé, la fonction ne refait pas les additions de
base. Elle efface les demi-mots `+0x18/+0x1A/+0x1C`, retire le bit `0x4000` à
`subrecord+0x0A`, puis répète la résolution. Cette seconde voie est un reset de
liaisons transitoires, pas un second format de fichier.

### Défixeur `0x823330F0`

`0x823330F0..0x82333200` effectue l'inverse structurel :

- si le bit 0 de `base+0x20` est posé, efface le pointeur transitoire à
  `subrecord+0x04` et le bit `0x4000` à `subrecord+0x0A` ;
- convertit les liens et pointeurs absolus en offsets relatifs ;
- soustrait `base` au premier mot de chaque entrée de table ;
- soustrait `base` aux champs `+0x0C`, `+0x10` et `+0x14` ;
- écrit finalement `0` à `base+0x20`.

La symétrie porte sur les mêmes comptes, strides, pointeurs, flags et ordre de
mutation. L'identification fixup/défixeur MATE passe donc de `probable` à
**`confirmed`** pour ce XEX.

## Propriétaire du type `0x0C`

Le handler `0x821C0E98..0x821C106C` parcourt deux vues de records et ne traite
que les records dont le byte de type vaut `0x0C`.

Pour chaque record admis :

1. `0x821BFFA0(this, record[1])` convertit l'identifiant secondaire en slot ;
2. le code indexe la table d'objets propriétaire avec `(slot + 0x24) * 4` ;
3. il récupère le bloc MATE depuis la vue ;
4. il appelle `0x82332E48` en mode `0` ou `1` ;
5. lorsque la validation réussit, il stocke le pointeur MATE fixé à
   `owner_object+0x30`.

Les sites directs du fixup observés sont `0x821C0F60`, `0x821C1048`,
`0x821C1A3C` et `0x821C1AEC`. Les deux premiers appartiennent au handler
ci-dessus ; les deux autres reproduisent la branche type `0x0C` dans le slot
virtuel `+0x1C`.

## RTTI et vtables

Le pointeur immédiatement avant la vtable dérivée établit la chaîne MSVC :

```text
vtable 0x820674D8
  <- CompleteObjectLocator 0x82078988
  -> TypeDescriptor 0x826EB05C
  -> ".?AVCX360ActorModelSetup@@"
```

La vtable de base adjacente établit :

```text
vtable 0x820674AC
  <- CompleteObjectLocator 0x820789D4
  -> TypeDescriptor 0x826EB080
  -> ".?AVCActorModelSetup@ACE6@@"
```

La chaîne globale déjà qualifiée reste :

```text
0x826A0728 -> 0x826A0708 -> vtable 0x820674D8
```

Le propriétaire concret du handler est donc **`CX360ActorModelSetup`**, dérivé
de **`ACE6::CActorModelSetup`**. Le cycle 136 classait ce nom `unknown`; cette
incertitude est désormais levée par RTTI, sans renommer les slots dont la
sémantique précise demeure inconnue.

## Frontière encore ouverte

`0x8233EE40` matérialise paresseusement le pointeur `subrecord+0x04` et pose le
bit `0x4000`. `0x8233EBB0` interroge le registre global `0x828C8100`; l'objet
obtenu reçoit ensuite un appel virtuel au slot byte `+0x04`.

Cette chaîne constitue le premier consommateur runtime confirmé conservant
l'identité d'un sous-record MATE. En revanche, les éléments suivants restent
`unknown` :

- classe concrète de l'objet résolu par la clé du sous-record ;
- signification des slots consommateurs byte `+0x10/+0x28/+0x2C` ;
- relation exacte avec technique, passe ou permutation Xenos ;
- draw causal utilisant le matériau.

Le verdict `NO_QUALIFIED_MATERIAL_BIND` du cycle 241 reste donc valide. Aucun
run humain n'est nécessaire : la prochaine étape est une remontée statique
bornée du registre et de la vtable résolus.

## Effet sur le code natif

Le parseur natif MATE consomme volontairement la représentation disque à
offsets relatifs. Ajouter maintenant une mutation fixup/défixeur dans
`mate.cpp` dupliquerait un détail du runtime invité sans consommateur natif.
Cette passe n'ajoute donc pas d'API de mutation. Le contrat sera matérialisé en
code seulement lorsqu'un runtime natif aura besoin d'une vue pointeur ou que
le résolveur de sous-records sera qualifié.

## Validation

Commandes exécutées :

```bash
sha256sum workspaces/ace-combat-6/game-files/default.xex
./.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -process default.xex -readOnly -noanalysis \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript DecompileMany.java 0x82332e50 0x823330f0 0x821c0e98 \
    0x821bffa0 0x8233ee40 0x8233ebb0 \
  -postScript DumpRange.java 0x82332e48 0x82333340 \
  -postScript DumpU32Range.java 0x820674a8 0x82067500 \
  -postScript DumpU32Range.java 0x82078980 0x820789f0 \
  -postScript DumpRange.java 0x821c0e98 0x821c10a0
```

- identité XEX : PASS ;
- analyse headless : PASS, code de sortie `0` ;
- journal : `artifacts/ac6-cycle242-headless.log` ;
- aucune modification native : le corpus CTest AC6 `44/44 PASS` du cycle 241
  reste la dernière validation de code applicable.

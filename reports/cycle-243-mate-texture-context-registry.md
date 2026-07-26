# AC6 cycle 243 — registre MATE des contextes texture

## Cible et méthode

- target : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- projet Ghidra canonique : `ace-combat-6`.

Cette passe est exclusivement statique, headless et en lecture seule. Elle ne
modifie ni le projet Ghidra, ni le XEX, ni AC6Recomp. Aucun Xenia, Wine, VNC,
GUI ou geste humain n'est utilisé.

## Résolveur et registre

`0x8233EBB0` conserve la clé 32 bits en `r4`, place l'adresse de sortie reçue en
`r5`, ajoute `0x80` au registre global `0x828C8100`, puis appelle
`0x8234AE00`. Il écrit le pointeur obtenu dans l'out-paramètre et retourne vrai
si ce pointeur est nul. Dans `0x8233EE40`, vrai signifie donc échec de
résolution et produit un retour nul.

`0x8234AE00` possède deux chemins :

- clé unsigned `< 0xFFFFFFF0` : entrée en section critique, recherche dans la
  table à `registry+0x100`, sortie de section critique ;
- clé `>= 0xFFFFFFF0` : index bas sur quatre bits dans un tableau de seize
  objets de stride `0x120`, dont la base est conservée à `registry+0x380`.

L'initialisation `0x8233E828` appelle le constructeur global, publie son état
prêt, puis initialise le service adjacent. Le constructeur brut initialise le
registre et les seize objets préalloués. La suppression différencie ces objets
inline des objets inscrits dans la table dynamique.

## RTTI qualifié

La vtable préallouée est `0x82010D10`. Son mot précédent pointe vers le
Complete Object Locator `0x8206A5D8`, qui référence le TypeDescriptor
`0x82676024` :

```text
.?AVTextureContextConcrete@Texture@NU@@
```

La hiérarchie MSVC référence également :

```text
NU::Texture::TextureContext
NU::Texture::ITextureContext
nuIUnknown
```

La classe de la ressource résolue est donc confirmée comme contexte texture
NU pour le pool préalloué. Le cycle 244 qualifie ensuite les RTTI, vtables,
constructeurs et la factory des trois classes adjacentes
`TextureContextXenon`, `TextureContextCubeMapXenon` et
`TextureContextMipMapXenon` comme entrées dynamiques possibles du registre.

## Correction du slot virtuel

Le dump d'instructions de `0x8233EE84..0x8233EE94` charge le premier mot de
l'objet, puis le mot à vtable byte `+0x04`. La cible correspondante dans
`0x82010D10` est `0x82352B58`, qui incrémente atomiquement le compteur à
`object+0x04`.

Le mot suivant, vtable byte `+0x08`, est `0x8234A950`. Il décrémente le même
compteur et, lorsque celui-ci atteint zéro, appelle le slot byte `+0x24` puis
retire l'objet du registre via `0x8233EBF0`.

Le cycle 242 attribuait par erreur l'appel de résolution au slot `+0x08`. Son
rapport contient désormais un erratum explicite. La séquence confirmée est :

```text
MATE subrecord key
  -> 0x8233EBB0 registry lookup
  -> NU::Texture::TextureContextConcrete-compatible object
  -> vtable byte +0x04 AddRef
  -> subrecord+0x04 transient pointer
  -> subrecord flag 0x4000
```

## Autres consommateurs bornés

Les appels directs du wrapper `0x8233EBB0` incluent :

- `0x82335F70`, qui invoque le slot byte `+0x10` ;
- `0x82337FF8`, qui invoque le slot byte `+0x28` ;
- `0x82337F00`, qui invoque le slot byte `+0x2C` avec la table globale
  `0x82871080` et met à jour des bits d'état après l'appel.

Ces arêtes confirment que le registre est utilisé comme interface de contexte
texture. Leur sémantique exacte, les objets Xenon concrets et le passage vers
une ressource GPU restent `unknown`; aucun nom de méthode plus précis n'est
attribué.

## Effet sur le natif

Le parseur MATE natif exposait déjà ces premiers mots sous le nom prudent
`texture_ids` et les recoupait avec les identifiants NDXR. La RTTI XEX confirme
ce choix de classe de ressource. Aucune nouvelle mutation native n'est
nécessaire : matérialiser des pointeurs invités ou une table de références
dupliquerait le runtime sans consommateur hôte.

Le README documente désormais cette provenance. Le verdict
`NO_QUALIFIED_MATERIAL_BIND` reste valable pour la sélection de shader et le
draw : un contexte texture n'est pas une technique, une passe ou une
permutation.

## Preuves

- `artifacts/ac6-cycle243-mate-resolver-pass1.log` ;
- `artifacts/ac6-cycle243-mate-resolver-pass2.log` ;
- `artifacts/ac6-cycle243-mate-resolver-rtti.log` ;
- `artifacts/ac6-cycle243-mate-resolver-type.log` ;
- `artifacts/ac6-cycle243-mate-resolver-hierarchy.log` ;
- `artifacts/ac6-cycle243-mate-resolver-construction.log` ;
- `artifacts/ac6-cycle243-texture-context-derived-rtti.log`.

Toutes les commandes utilisent `analyzeHeadless -readOnly -noanalysis` sur le
projet canonique et terminent avec code `0`.

## Limites et prochaine étape

Cette passe ne qualifiait pas encore :

- la classe de chaque entrée dynamique du registre ;
- les contrats exacts des slots byte `+0x10/+0x28/+0x2C` ;
- le passage du contexte vers une texture Xenos ;
- une technique, une passe, une permutation ou un draw causal ;
- la compatibilité avec un autre XEX ou une autre région.

Le cycle 244 ferme les RTTI, vtables, constructeurs, factory et contrats bornés
de ces slots. La prochaine passe doit partir du champ ressource opaque `+0x50`
et des callees qualifiés, sans perdre l'identité de la clé MATE. Aucune action
humaine n'est demandée.

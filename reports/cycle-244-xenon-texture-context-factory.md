# AC6 cycle 244 — factory et contrats `TextureContext*Xenon`

## Cible et portée

- target : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- projet Ghidra canonique : `ace-combat-6`.

Cette passe est statique, headless, `-readOnly -noanalysis`. Elle prolonge le
cycle 243 sans relancer le fixup MATE, sans Xenia et sans action humaine.

## RTTI et vtables

Le nouveau script versionné `FindMsvcRttiVtables.java` recherche un
TypeDescriptor exact, son Complete Object Locator, puis l'address-point de sa
vtable. Il reproduit d'abord le résultat connu de
`TextureContextConcrete`, puis ferme les trois types Xenon :

| TypeDescriptor | Complete Object Locator | vtable | type RTTI |
| --- | --- | --- | --- |
| `0x826784F0` | `0x8206B1A8` | `0x820125B8` | `TextureContextXenon` |
| `0x82678524` | `0x8206B1FC` | `0x820125F4` | `TextureContextCubeMapXenon` |
| `0x82678558` | `0x8206B250` | `0x82012690` | `TextureContextMipMapXenon` |

Les trois vtables partagent les slots de base jusqu'au byte `+0x18`, dont
`+0x04=0x82352B58` (incrément de référence), `+0x08=0x8234A950`
(libération) et `+0x10=0x8234AA08`. Les slots byte `+0x24` et `+0x28` sont
également communs, à `0x8234EF58` et `0x8234FDA0`. Le slot byte `+0x2C` est
`0x8234EFA8` pour les variantes Xenon et CubeMap, mais `0x8234FDE8` pour la
variante MipMap.

## Constructeurs et factory du registre

Les trois constructeurs appellent le constructeur commun `0x8234AA68`,
mettent le champ `object+0x50` à zéro et publient leur vtable :

| constructeur | vtable publiée | classe qualifiée |
| --- | --- | --- |
| `0x8234EBA8` | `0x820125B8` | `TextureContextXenon` |
| `0x8234EEC8` | `0x820125F4` | `TextureContextCubeMapXenon` |
| `0x8234FB08` | `0x82012690` | `TextureContextMipMapXenon` |

Dans la factory `0x8234AED8`, une clé normale unsigned `< 0xFFFFFFF0` est
conservée en `r30`. Deux sélecteurs décident du constructeur :

| premier sélecteur | second sélecteur | résultat |
| --- | --- | --- |
| `0` | `1` | `TextureContextXenon` |
| `0` | autre | `TextureContextMipMapXenon` |
| non-zéro | `1` | `TextureContextCubeMapXenon` |
| non-zéro | autre | aucun objet |

L'objet obtenu est inscrit avec sa clé par `0x8234AE78`. Les clés réservées
`>= 0xFFFFFFF0` utilisent le pool inline du cycle 243 et sont reconstruites
comme `TextureContextXenon`. Cela confirme que les entrées dynamiques du
registre peuvent être les trois classes Xenon ci-dessus ; la clé MATE reste
inchangée pendant cette sélection.

## Contrats virtuels bornés

- byte `+0x10`, `0x8234AA08` : copie les mots `object+0x0C` et
  `object+0x10` vers deux sorties. Leur sémantique reste `unknown` ;
- byte `+0x24`, `0x8234EF58` : si le champ `object+0x50` est non nul et que
  `object+0x18` vaut zéro, appelle `0x8233BE38`, puis remet `+0x50` à zéro ;
- byte `+0x28`, `0x8234FDA0` : appelle `0x821E1088` avec le mot à
  `argument+0x04`, l'index reçu, `object+0x1C` et un masque 64 bits dérivé de
  cet index ;
- byte `+0x2C`, `0x8234EFA8` : répète le même appel à `0x821E1088`, pose un
  bit dans une table indexée de stride `0x18`, agrège le masque dans le mot
  64 bits à `owner+0x18`, puis appelle `0x821DC4F8` et `0x821DC688` ;
- byte `+0x2C`, `0x8234FDE8` : suit le même préambule pour MipMap, puis
  transmet en plus un paramètre normalisé à au moins `1` à `0x821DC908`.

Ces contrats qualifient les accès et appels, pas leurs noms métier. Le
décompilateur ne reconnaît pas correctement plusieurs prologues Xenon ; les
dumps d'instructions sont donc la preuve primaire pour `+0x2C`.

## Champ ressource et limite actuelle

Le champ `object+0x50` était le premier candidat de ressource concret : il est
initialisé à zéro par chaque constructeur, écrit par des chemins propres aux
classes et effacé par le slot `+0x24`. Les preuves présentes ne permettent pas
encore de dire s'il s'agit d'un pointeur, d'un handle D3D/Xenos ou d'un autre
identifiant opaque. Il reste nommé **champ ressource opaque `+0x50`**.

Le cycle 245 supersède cette limite : les six producteurs prouvent un mot
32 bits de stockage sous-jacent, soit alloué, soit dérivé d'un descripteur
source. Son typedef XDK exact et son bind Xenos restent inconnus.

Le cycle ferme donc le raccord RTTI/factory demandé au cycle 243 et borne les
trois slots consommateurs. Il ne ferme pas encore la jointure vers une texture
GPU, un shader, une permutation ou un draw. Le verdict
`NO_QUALIFIED_MATERIAL_BIND` reste inchangé.

## Preuves et validation

- `artifacts/ac6-cycle244-texture-context-vtables.log` ;
- `artifacts/ac6-cycle244-texture-context-vtable-layout.log` ;
- `artifacts/ac6-cycle244-texture-context-constructors.log` ;
- `artifacts/ac6-cycle244-texture-context-factory.log` ;
- `artifacts/ac6-cycle244-texture-context-resource-field.log` ;
- `artifacts/ac6-cycle244-texture-context-slot28-2c.log` ;
- `artifacts/ac6-cycle244-rtti-script-validation.log` ;
- `workspaces/ace-combat-6/scripts/FindMsvcRttiVtables.java`.

Toutes les passes headless terminent avec code `0`. La validation finale
reproduit les quatre RTTI connus et retourne explicitement
`col=none vtable=none` pour le contrôle négatif `0x82000004`. Aucun code natif
n'est modifié : le parseur conserve correctement la clé sous `texture_ids`, et
un champ invité opaque ne doit pas être matérialisé sans consommateur hôte.

## Prochaine frontière

Le cycle 245 ferme les producteurs de `+0x50`. La prochaine frontière part du
descripteur `object+0x1C` vers son premier bind Xenos, uniquement tant que
l'identité de la clé ou de l'objet est conservée. Aucune session humaine n'est
demandée.

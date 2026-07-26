# AC6 cycle 245 — stockage sous-jacent des contextes texture Xenon

## Identité et méthode

- target : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- projet Ghidra canonique : `ace-combat-6`.

L'analyse est exclusivement headless, statique, `-readOnly -noanalysis`. Une
seconde inspection indépendante en lecture seule a recoupé les bornes et les
provenances. Aucun Xenia, VNC, GUI ou geste humain n'est utilisé.

## Correction du statut de `object+0x50`

Le cycle 244 laissait `object+0x50` comme champ ressource opaque. Les six
méthodes spécifiques des trois classes Xenon ferment maintenant sa provenance :
il s'agit d'un **mot 32 bits d'adresse/stockage sous-jacent**, et non d'une clé
MATE, d'un identifiant de shader ou d'une permutation.

Deux chemins le produisent :

1. les méthodes au vtable byte `+0x1C` obtiennent une taille/disposition,
   allouent via `0x8233BE20(..., 0x1000)`, stockent le résultat à `+0x50`,
   ajustent le bloc descripteur à `object+0x1C`, puis remplissent le stockage ;
2. les méthodes au vtable byte `+0x20` stockent l'adresse renvoyée par
   `0x8234B268(source_descriptor, selector, 0)`, puis ajustent le même bloc
   descripteur.

`0x8233BE20` délègue au gestionnaire global `0x82910C80`; la libération
`0x8233BE38` utilise le même gestionnaire. Le slot commun byte `+0x24`,
`0x8234EF58`, libère `+0x50` seulement lorsque `object+0x18 == 0`, puis le
remet systématiquement à zéro. L'ownership public exact reste inconnu.

## Arithmétique source de `0x8234B268`

Pour le descripteur reçu en `r3`, le sélecteur en `r4` et le niveau en `r5`,
le pointeur initial vaut :

```text
selector == 1 : descriptor + zero_extend_u16(descriptor + 0x0C)
selector == 2 : descriptor + u32(descriptor + 0x20)
otherwise     : descriptor
```

Si le niveau est non nul, la routine ajoute ensuite les mots de la table à
`descriptor+0x30`, un par niveau. Les trois méthodes byte `+0x20` passent
explicitement un niveau nul : leur valeur stockée à `+0x50` est donc exactement
l'une des trois expressions ci-dessus.

## Méthodes qualifiées

| classe | slot | intervalle | contrat borné |
| --- | --- | --- | --- |
| `TextureContextXenon` | `+0x1C` | `0x8234EC38..0x8234ED84` | allocation, ajustement du descripteur et un remplissage |
| `TextureContextXenon` | `+0x20` | `0x8234EE48..0x8234EEC4` | adresse dérivée du descripteur source |
| `TextureContextCubeMapXenon` | `+0x1C` | `0x8234F148..0x8234F27C` | allocation commune puis exactement six remplissages, un par face |
| `TextureContextCubeMapXenon` | `+0x20` | `0x8234F0D0..0x8234F144` | adresse dérivée du descripteur source |
| `TextureContextMipMapXenon` | `+0x1C` | `0x8234FB98..0x8234FD98` | allocation, niveau initial puis niveaux suivants |
| `TextureContextMipMapXenon` | `+0x20` | `0x8234FE98..0x8234FF24` | adresse dérivée du descripteur source |

Le chemin CubeMap boucle six fois. Le chemin MipMap lit l'octet
`source_descriptor+0x11` via `0x8234B128` comme borne de niveaux, remplit le
niveau initial puis les niveaux `1..<count`. Les offsets de face/niveau sont
calculés avant d'être ajoutés au mot `+0x50` et transmis comme adresse de
destination à `0x821FCA48`.

Le getter `0x8234ED88` charge directement `object+0x50`, l'écrit dans
l'out-paramètre en `r6` lorsqu'il est non nul et retourne un code d'échec fixe
sinon. Cela confirme que le champ est exposé comme valeur d'adresse 32 bits,
pas comme objet virtuel.

## Bloc descripteur adjacent

Les six chemins construisent ou utilisent le bloc à `object+0x1C` :

- `0x821FBE30` et `0x821FBF30` préparent sa disposition selon la famille ;
- `0x821FC070(object+0x1C, backing_word)` incorpore la valeur de stockage dans
  des champs dépendant du format ;
- `0x821FCA48` remplit le stockage avec les dimensions, format et offsets
  calculés.

La structure est donc un descripteur texture Xenon avec un stockage séparé au
niveau structurel. Son typedef XDK exact, sa relation à un objet D3D publié et
le point où elle est liée à un stage Xenos restent `unknown`; aucun nom d'API
XDK n'est imposé par similarité seule.

## Effet sur le natif

Aucune structure invitée n'est ajoutée au runtime natif. Le parseur MATE doit
continuer à conserver la clé 32 bits sous `texture_ids`; l'adresse `+0x50` est
un produit runtime du XEX et n'appartient pas au format sérialisé MATE.

Le verdict `NO_QUALIFIED_MATERIAL_BIND` reste valide : avoir qualifié le
stockage et le descripteur ne prouve toujours ni le stage, ni le shader, ni la
permutation, ni le draw qui les consomme.

## Preuves et validation

- `artifacts/ac6-cycle245-resource-producer-ranges.log` ;
- `artifacts/ac6-cycle245-resource-helper-decompile.log` ;
- `artifacts/ac6-cycle245-resource-helper-ranges.log` ;
- `artifacts/ac6-cycle245-texture-backing-validation.log` ;
- `workspaces/ace-combat-6/scripts/VerifyTextureContextBackingStore.java`.

Le vérificateur réexécutable contrôle 19 instructions critiques : allocations,
adresses dérivées, stores/loads `+0x50`, ajustement du descripteur, remplissage,
getter et nettoyage. Il passe sur le projet canonique et la passe headless
termine avec code `0`.

## Prochaine frontière

Partir du bloc descripteur `object+0x1C` et des helpers
`0x821FBE30/0x821FBF30/0x821FC070`, puis rechercher son premier consommateur
qui fixe un stage, un sampler ou une ressource Xenos. La recherche doit rester
bornée aux flux conservant l'objet, son descripteur ou sa clé MATE. Une absence
doit produire une frontière négative précise, pas un nouveau scan global et
pas une demande de session humaine.

Le cycle 246 ferme cette frontière : le slot byte `+0x28` transmet le
descripteur à `0x821E1088`, qui le copie dans le shadow state texture indexé
par stage. Il révèle aussi que le hook de référence à `0x821E10C8` est placé au
milieu de cette fonction après mutation de `r4`.

# Frontière statique objet NDXR → paire VS/PS

## Résultat

Une correspondance exacte est démontrée pour 169 des 170 objets `mapparts` :

```text
objet NDXR
  -> descriptor+0x10 : matériau
  -> material+0x00 = 0x30000010
  -> registre NSXR 0x8296BF80
  -> NSXR entry_0163..., membre 0, descripteur @0x2100
  -> VS vsCstCT.updb + PS psCT.updb
```

Chaque descripteur NSXR est parcouru depuis `NSXR+0x20`; sa clé est le mot à
`+0x00`, son suivant est `current + u32(+0x18)`. Le descripteur
`0x30000010` contient exactement un enregistrement vertex puis un pixel.

## Chaîne interprocédurale minimale

1. **PROUVÉ** — `FUN_822ECA00` reconnaît `NSXR`.
2. **PROUVÉ** — `Function_822E0950` lit `u16 NSXR+0x0A`, commence à `+0x20`,
   avance de `descriptor+0x18` et obtient la clé par `FUN_822DFB30`.
3. **PROUVÉ** — `FUN_822DFB30` retourne `u32 descriptor+0x00`.
4. **PROUVÉ** — `Function_822EC730` insère/résout cette clé dans le registre
   `0x8296BF80` via `Function_822E6A98`/`Function_822E6A38`.
5. **PROUVÉ** — `Function_821A2680` charge la banque NSXR de l'entrée DATA
   `0xA3` et appelle le chargeur pour ses membres.
6. **PROUVÉ** — `Function_822E8668` lit `material+0x00`, interroge le même
   registre `0x8296BF80`, cache le pointeur à `+0x04` et arme le bit résolu.
7. **PROUVÉ** — `Function_822E1560` lie déclaration vertex, textures et shader
   dans le même contexte de draw. Il relit une clé alternative à
   `material+0x14` lorsque la condition de passe le demande.

## Passe et variantes

La banque contient aussi :

```text
0x30040010 -> entry_0163..., membre 7, @0x2300
           -> vsCstCT.updb + psCT.updb (répertoire Common_HDR)
```

Le delta exact est `0x00040000`, tandis que les bits de variante bas et les
noms VS/PS restent identiques. **FORTEMENT ÉTAYÉ** : la passe HDR est un axe
orthogonal de la clé. **INCONNU** : le writer qui place la clé alternative à
`material+0x14` et le prédicat de `FUN_822DBAA8`; on ne prétend donc pas encore
que chaque objet observé choisit effectivement HDR.

Les clés terrain `0x04100002` (Map) et `0x04140002` (Map_HDR) présentent le
même delta `0x00040000`, ce qui corrobore indépendamment ce découpage.

## Hypothèses réfutées

- **RÉFUTÉ** — `NU_HASH` comme clé brute NSXR : zéro correspondance sur 170.
- **RÉFUTÉ pour `0x30000010`** — choix de `psMapCTF2_*`,
  `psCTF_ExpFog_MPARTS2`, `psMapOcean` ou `psOcnT` depuis les seuls slots :
  la clé matériau rejoint directement `psCT`.
- **RÉFUTÉ pour cette classe** — recomposition obligatoire de la clé depuis
  flags, textures et attributs au moment du draw. Ces données sont consommées
  séparément et les variations observées ne changent pas `material+0x00`.
- **RÉFUTÉ** — l'immédiat `0x30000010` à `0x821227C0` comme parser matériau :
  `Function_821225B8` l'emploie pour configurer une ressource GPU.

## Frontière restante exacte

Un seul objet, `mapparts_m01_m_021_60_O_OBJ`, utilise cinq matériaux
`0x30000090`. La clé et sa variante supposée `0x30040090` sont absentes des 51
NSXR inventoriés. La prochaine expérience statique au meilleur pouvoir
discriminant est bornée à :

1. décompiler `Function_822E8488`, appelée juste après la résolution de la clé
   primaire, et recenser ses writers de `material+0x14` ;
2. décompiler `Function_822EDF60` pour publier la déclaration canonique de
   `06/13` ;
3. expliquer le traitement du bit `0x80` de l'unique clé `0x30000090`.

`done_when` : valeur écrite à `material+0x14`, masque éventuel de `0x80` et
éléments de déclaration `06/13` observés, ou preuve négative bornée identifiant
un autre registre. Aucun runtime n'est requis à cette frontière.


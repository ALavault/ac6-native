# Layout matériau NDXR de la démo PAL

## Qualification

Projet Ghidra canonique : `ghidra-projects/ace-combat-6-demo`, module
`Default.xex` de la démo PAL qualifiée. Le corpus est celui de
`ac6_demo_work/ac6-static-analysis`, issu du même XEX démo et de ses données.
Les adresses retail antérieures ne sont pas utilisées comme preuve.

## Layout brut

Le pointeur de matériau est lu dans chacun des quatre slots
`descriptor+0x10..+0x1C`. Dans 169 des 170 objets `mapparts`, il vise :

| Offset | Taille | Valeur/rôle | Preuve |
|---:|---:|---|---|
| `+0x00` | 4 | clé shader `0x30000010` | **PROUVÉ** : valeur brute et clé NSXR identique |
| `+0x04` | 4 | cache pointeur, nul sur disque | **PROUVÉ** par `Function_822E8668` |
| `+0x08` | 2 | flags runtime ; bit `0x4000` = résolu | **PROUVÉ** par `Function_822E8668` |
| `+0x0A` | 2 | nombre de références texture | **PROUVÉ** par `Function_822E1560` |
| `+0x0C` | 2 | état matériau, sens fin inconnu | **CANDIDAT** |
| `+0x0E` | 2 | état utilisé par le rendu, sens fin inconnu | **CANDIDAT** |
| `+0x10` | 4 | champ auxiliaire | **INCONNU** |
| `+0x14` | 4 | clé shader alternative consultée au rendu | **PROUVÉ** comme lecture ; writer **INCONNU** |
| `+0x20` | `0x18*N` | références texture | **PROUVÉ** |
| après textures | variable | chaîne de paramètres nommés | **PROUVÉ** |

Correction importante : le compte n'est pas l'entier 32 bits à `+0x08`.
L'image `00 00 00 01` représente deux halfwords : flags `0` à `+0x08`, puis
compte `1` à `+0x0A`.

Une référence texture de `0x18` octets est consommée ainsi :

| Offset | Rôle | Consommateur |
|---:|---|---|
| `+0x00` | GIDX / clé texture | `Function_822E8520` via `Function_822E1560` |
| `+0x04` | cache pointeur résolu | `Function_822E8520` |
| `+0x0A` | flags, bit `0x4000` = résolu | `Function_822E8520` |
| `+0x0C..+0x0E` | trois champs 3 bits copiés dans l'état sampler | `Function_822E1560` |
| `+0x12` | octet initialisé à 1 si nul | `Function_822E1560` |
| `+0x14` | argument transmis au binder texture | `Function_822E1560` |

## Paramètres `NU_*`

`NU_FLAG1` et `NU_FLAG2` ne sont pas des membres fixes de la référence texture.
Ce sont des nœuds nommés de `0x20` octets placés après le tableau texture :

| Offset du nœud | Rôle |
|---:|---|
| `+0x00` | taille/type `0x20` |
| `+0x04` | offset relatif du nom |
| `+0x08` | taille du payload, `4` |
| `+0x10` | valeur big-endian |

Pour un matériau à une texture, le premier nœud commence à `material+0x38` ;
dans l'exemple à `NDXR+0x5D0`, `NU_HASH`, `NU_FLAG1` et `NU_FLAG2` sont à
`+0x38`, `+0x58` et `+0x78`. Sur 170 objets, `(FLAG1,FLAG2)` vaut `(0,0)` 153
fois et `(1,0)` 17 fois.

## Consommateurs et conclusion

- **PROUVÉ** — `Function_822E8668` résout `material+0x00` dans le registre
  NSXR `0x8296BF80`, met le pointeur à `+0x04` et arme `+0x08 & 0x4000`.
- **PROUVÉ** — `Function_822E1560` parcourt `material+0x0A` références à
  partir de `+0x20`, stride `0x18`, puis lie les textures.
- **PROUVÉ** — les 169 objets ordinaires gardent la clé `0x30000010`, que
  `FLAG1` soit 0 ou 1. Ces flags ne sélectionnent donc pas la paire de base de
  cette classe.
- **RÉFUTÉ** — `NU_HASH` n'est pas une clé NSXR brute : aucune des 170 valeurs
  n'apparaît dans les 51 conteneurs NSXR inventoriés.
- **INCONNU** — l'objet exceptionnel `mapparts_m01_m_021_60_O_OBJ` porte
  `0x30000090` dans cinq matériaux ; cette clé est absente des NSXR inventoriés.
  C'est aussi l'unique jointure GIDX obtenue par fallback aligné.


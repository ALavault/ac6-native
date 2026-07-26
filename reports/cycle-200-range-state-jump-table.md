# AC6 cycle 200 — table de sauts des états de bornes

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- image base : `0x82000000`

Passe PPC statique headless en lecture seule sur le projet Ghidra canonique.
Les scripts sont `DumpU32Range.java` et `DumpRange.java`. Aucun état Ghidra,
binaire, sortie générée ou configuration n'a été modifié.

## Table de dispatch

Le flux autour de `0x8226e368` lit l'état `objet+0x44`, refuse les valeurs
supérieures à `4`, puis indexe une table à `0x8226e38c` avec un pas de quatre
octets :

| index `objet+0x44` | cible | effet principal |
|---:|---:|---|
| `0` | `0x8226e3a0` | met `+0x48`, `+0x4c` et `+0x54` à `0.0` |
| `1` | `0x8226e3b0` | `+0x48 = objet+0x30 + DAT_82008134`, `+0x50 = objet+0x28c - DAT_82008134`, `+0x4c/+0x54 = 0` |
| `2` | `0x8226e3e0` | `+0x48 = objet+0x30 - DAT_82008134`, `+0x50 = objet+0x294 + DAT_82008134`, `+0x4c/+0x54 = 0` |
| `3` | `0x8226e438` | `+0x4c = objet+0x34 + DAT_82008134`, `+0x54 = objet+0x290 - DAT_82008134`, `+0x48/+0x50 = 0` |
| `4` | `0x8226e410` | `+0x4c = objet+0x34 - DAT_82008134`, `+0x54 = objet+0x298 + DAT_82008134`, `+0x48/+0x50 = 0` |

Chaque handler rejoint `0x8226e464`. Les champs produits sont donc quatre
scalaires bornés organisés en deux paires, et non une simple copie du record.
Le facteur `DAT_82008134` est conservé comme constante non nommée : sa valeur
et sa sémantique ne sont pas nécessaires pour établir le contrat ABI.

## Suite commune au dispatch

Après le handler, le flux lit `record+0x20` et peut appeler
`0x822667c8` lorsque cette valeur dépasse le flottant de référence. Il
consulte ensuite un état global de mode et une table de sous-traitement. Cette
suite reste un service de transition/intervalle ; aucun accès direct à un
contexte d'avion ou à un writer de pose n'est présent dans la tranche observée.

Le record source provient du pointeur déjà observé à `objet+0x268`. Ses champs
`+0x10..+0x1c` alimentent `objet+0x30..+0x3c`, tandis que ses champs
`+0x00..+0x0c` servent au calcul de bornes par `0x82268b28`. La table de
sauts ne change donc pas l'identité du record : elle choisit une combinaison
de bornes selon l'état déjà classé à `+0x44`.

## Qualification

- `confirmed` : base et cinq entrées de la table, garde `state <= 4`, offsets
  d'entrée/sortie, calculs flottants et jonction commune `0x8226e464` ;
- `cross-match` : classifieur de bornes/intervalle pour une transition ou un
  paramètre de scène ;
- `unknown` / `needs-dynamic-evidence` : signification gameplay des paires,
  relation éventuelle à une caméra, une trajectoire ou un avion.

Ne pas nommer `+0x48/+0x4c/+0x50/+0x54` `position`, `altitude`, `heading` ou
`speed`. Les noms offset/role (`range_state_value_a`, etc.) restent plus
fidèles tant qu'une trace n'a pas corrélé les valeurs à un objet du jeu.

Aucune action humaine n'est requise pour ce cycle. Une future session Xenia
sera une preuve de sémantique, pas une condition pour poursuivre la
transcription statique.

## Validation

- `DumpU32Range.java 0x8226e38c 0x8226e3c0` ;
- `DumpRange.java 0x8226e3a0 0x8226e4a0` ;
- `DumpRange.java 0x8226e460 0x8226e640` ;
- CTest AC6 : gate connu `41/41` ;
- launcher Xenia/Wine : `status=ready`, `release=16e1eb8`,
  `renderer=vulkan`.


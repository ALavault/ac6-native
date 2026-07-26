# Cycle 265 — qualification de l'audit des limites de fonctions AC6

## Question bornée

L'archive externe `ac6_function_boundary_audit_v1.zip` apporte-t-elle une
preuve suffisante pour retirer les trois départs TOML qui coupent la fonction
réelle `0x823849C8..0x82384AEF`, sans relancer une boucle coûteuse de
régénération et de smoke ?

## Identité

- cible : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 XEX local :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- archive : `ac6_function_boundary_audit_v1.zip` ;
- SHA-256 archive :
  `acddf67df1ceb61f02201f4aff8759c12c8f977afc620d892ed656fb6679bdd4`.

Le TOML local et l'unité générée locale concordent exactement avec les hashes
attendus par l'archive :

| Entrée | SHA-256 | Taille |
|---|---|---:|
| `ac6recomp_config.toml` avant patch | `0c90eb348404518221098e927f0f7c37973c49ea3ab2b13da2c7ae7442f04a0c` | 462 376 |
| `generated/ac6recomp_recomp.41.cpp` | `8fa25c3e80d9c86dd31bae7c4dca2597ce0ea41d413506eeedf7ba68c1179852` | 2 090 805 |

Le log Ghidra local a grandi depuis le snapshot de l'archive : son hash courant
est `bb7975156a46828a9050847f7243af89a2c45034e18d6b8ed7925ef082679a70`
et sa taille 9 322 octets, au lieu du hash source annoncé par l'archive. Cette
dérive n'est pas silencieusement assimilée à une identité exacte. La ligne
qualifiante reste toutefois présente dans le log courant et donne toujours :

```text
823849c8 Function_823849C8 [[823849c8, 82384aef]]
82384af0 Function_82384AF0 [[82384af0, 82384b2f]]
```

L'export headless courant `exports/823849c8.json` confirme la même fonction et
`exports/82384af0.json` confirme l'entrée autonome suivante.

## Preuve de coupure

L'unité générée contient déjà `loc_82384A88` dans le corps principal, mais
transforme encore la branche arrière `0x82384AD0 -> 0x82384A88` en
`REX_FATAL`. Les trois entrées configurées recouvrent des fragments du même
corps :

| Entrée TOML | Preuve entrante | Verdict |
|---|---|---|
| `0x823849F0` | fallthrough depuis `0x823849EC`, frame et `r31` hérités | `confirmed_internal_split` |
| `0x82384AAC` | fallthrough depuis `0x82384AA8`, `CR6` et registres de boucle vivants | `confirmed_internal_split` |
| `0x82384AE8` | fallthrough depuis `0x82384AE4`, fin d'épilogue seulement | `confirmed_internal_split` |
| `0x82384AF0` | fonction Ghidra autonome avec prologue/consommateurs propres | `confirmed_real_entry` |

Les trois faux départs ont été retirés uniquement de
`.tools/ac6-recomp-reference/ac6recomp_config.toml`. La sortie générée n'a pas
été modifiée manuellement.

## Validation exécutée

Validation volontairement peu coûteuse :

- `unzip -t ac6_function_boundary_audit_v1.zip` : PASS, 16 membres ;
- `sha256sum -c SHA256SUMS` après extraction temporaire : 15/15 PASS ;
- contrôle des hashes XEX/TOML/unité générée : PASS ;
- contrôle headless de la plage Ghidra courante : compatible ;
- vérification que `0x82384AF0` reste configurée : PASS.

La régénération XenonRecomp, le build, CTest et le smoke retail n'ont pas été
exécutés conformément à la consigne de sauter les validations coûteuses pour
cette passe. Le front runtime suivant reste donc `unknown`, et non
`0x82384DE4` par simple ordre textuel.

## Données retail lourdes et revue ChatGPT

`DATA00.PAC` fait 2 267 086 848 octets et `DATA01.PAC` 664 141 824 octets.
Ils ne doivent pas être joints à un paquet ChatGPT. Si une future adresse
logique se résout dans un de ces conteneurs, le paquet doit contenir un
manifeste de plages avec, pour chaque tranche :

```text
container_path
container_sha256
offset_hex
length_bytes
context_before_bytes
context_after_bytes
reason
expected_parser
slice_sha256
```

Seules les tranches localement extraites et strictement nécessaires seront
allowlistées. Une tranche reste une preuve dérivée liée au hash du conteneur ;
elle ne doit pas être présentée comme un fichier retail autonome complet.

## Frontière suivante

La prochaine action native est une régénération unique, puis un smoke borné
qui vérifie la disparition exacte de :

```text
Unresolved branch from 0x82384AD0 to 0x82384A88
```

Cette action est différée. Si le smoke révèle ensuite une ressource issue de
`DATA00.PAC` ou `DATA01.PAC`, relever d'abord l'offset fichier et la longueur,
puis produire un paquet de tranches borné au lieu du conteneur complet.

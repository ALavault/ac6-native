# Cycle 1703 — jointure statique de la table XMA (correction du cycle 1702)

## Verdict

La capture du cycle 1702 ne doit pas être appelée « descripteur de 96 octets ».
Le code PAL qualifié passe à `XMACreateContext` l’adresse d’un mot de sortie
situé à `entry + 64`; la sonde a seulement lu une fenêtre bornée de 96 octets à
cette adresse. Cette fenêtre contient donc le mot de sortie et peut traverser
la fin de l’entrée puis l’entrée suivante. L’import reste fail-closed et aucun
état XMA n’est injecté.

## Identité et preuves

| élément | valeur |
|---|---|
| cible | `Default.xex` démo PAL |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| basefile SHA-256 | `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| sonde | `AC6_DEMO_WATCH_XMA_CREATE=1`, lecture seule |
| fenêtre observée | base `0x17360050`, longueur `0x60`, SHA `a4f014e03e752249dc9740522458b6fc4d212d013add1e7f2a1d99cc3346dedf` |
| routes | neutral et START frais, tick 1048, thread 21 |
| import | `xboxkrnl.exe:XMACreateContext`, ordinal 548, LR `0x82357298` |

La fenêtre commence par `0x00000000`; ce zéro est une valeur **pré-appel**
observée, pas une valeur de retour. Le trap intervient avant toute écriture
par l’import.

## Structure démontrée par les bytes PAL

La fonction `0x82357240..0x8235730B` (taille `0xCC`, bytes SHA-256
`7436f8404267283916f2f2e64fdcda534788553fbf366daa330bc09fe9220ed9`, pseudocode
SHA-256 `9d2db824bcf8391231e9b809524e7acec3079f2204cfd3c28dd3e8ceda002a13`)
répète exactement, pour un index `i` :

```text
table       = r3
count       = u32be(table + 0)
flags       = u32be(table + 4)
entry_base  = u32be(table + 8) + i * 96
output_slot = entry_base + 64
XMACreateContext(output_slot)
```

Après un retour non négatif, le code relit `u32be(output_slot)`, appelle
`MmGetPhysicalAddress`, écrit un `u16` à `entry_base + 80`, puis publie un bit
calculé à partir de cette valeur via un store MMIO. Ces opérations sont des
faits structurels; la signification audio des champs reste inconnue.

La sonde ayant reçu `r3=0x17360050`, le candidat `entry_base=0x17360010`
correspond à `i=0` et à la formule ci-dessus, mais l’index et le pointeur
`table+8` n’ont pas été enregistrés : ce candidat n’est pas promu comme
adresse de table.

## Consommateurs et producteurs statiques bornés

| fonction PAL | faits observés | statut |
|---|---|---|
| `0x823567E0` | boucle sur `count`, charge `entry+64`, appelle `XMAReleaseContext` (ordinal 550), puis remet `entry+64` à zéro; efface un bit de `table+4` | `demo-qualified` structurel |
| `0x82356868` | calcule `entry_base` avec `i*96`; selon deux bits de `entry+0`, appelle `MmGetPhysicalAddress`; écrit `+20`, `+24`, `+84`, `+88`, modifie `+0`, `+4`; peut lire `*r5` et modifier `entry+8` | `demo-qualified` structurel |
| `0x82356940` | compare `entry+84` ou `entry+88` à `r5`, retourne 0/1 | `demo-qualified` structurel |
| `0x823569A0`, `0x823569F0` | tests de bits et lecture de l’état d’une entrée ou du contexte pointé | `demo-observed`, sémantique inconnue |
| `0x82357030` | transforme un mot borné en une valeur dérivée; appelée par `0x82356868` | `demo-qualified` structurel |
| `0x82357310` | parcourt les entrées, écrit des bits calculés depuis `entry+80` vers une zone MMIO, puis modifie `table+4` | `demo-qualified` structurel |

Les hashes de frontières et de pseudocode de ces fonctions sont consignés dans
[la capsule](../analysis/demo/ac6-demo-xma-table-static-join-v1.json); aucune
borne issue d’un nom C++ généré n’est utilisée.

## Correction et garde

- `demo-qualified` : appel, ordinal, LR, tick/thread, adresse `r3`, mot
  pré-appel et fenêtre bornée; formule de stride 96 et offset 64; écritures et
  appels statiques listés ci-dessus.
- `demo-observed` : fenêtre de 96 octets identique neutral/START.
- `xenia-generic` : prototype générique de `XMACreateContext` dans Xenia,
  sans promotion dans l’ABI PAL.
- `unknown` : valeur de retour, allocation, contenu post-appel de
  `output_slot`, champs audio, packets/timestamps/volume et consommation.

La garde reste inchangée : ordinal 548 trap avant effet. Aucun décodage
FFmpeg/vgmstream, readback ou screencap n’est autorisé à cette frontière.

## Prochain checkpoint minimal

Sur deux exécutions fraîches neutral/START, observer en lecture seule le mot
`[r3, r3+4)` et les premiers stores/readbacks de l’entrée, avec PC/LR/thread/
tick. L’observation doit distinguer `entry+64` de la fenêtre adjacente et ne
doit pas appeler l’import. Tant qu’un output pointer stable et un premier
paquet XMA exact ne sont pas qualifiés, le service audio reste bloqué.


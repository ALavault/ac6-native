# Cycle 185 — owner global et slot virtuel `+0x6c`

Date : 2026-07-18 (Europe/Paris)

## Cible

- target ID : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- projet Ghidra : `ace-combat-6`
- base : `0x82000000`

Passe headless statique, lecture seule, sans intervention humaine.

## Chaîne du global owner

Le corps `0x820f8df0` utilise le global `0x823f9b28` à plusieurs endroits,
notamment au site `0x820f8e4c` :

```text
0x823f9b28 -> 0x823f9b2c
0x823f9b2c + 0x00 -> 0x8205d054
```

Le premier mot de l'objet statique pointé est donc `0x8205d054`, qui forme
l'address-point/vtable initiale observable dans le XEX. Au slot `+0x6c` :

```text
0x8205d054 + 0x6c = 0x8205d0c0 -> 0x82266390
```

Le dispatch du corps est :

```text
lwz r3, -0x64d8(r30)     # charge le global owner
lwz r11, 0(r3)           # charge son address-point
lwz r11, 0x6c(r11)       # slot virtuel +0x6c
mtspr CTR, r11
bctrl
```

## Séparation avec les tables du cycle 183

Les deux tables statiques contenant `0x82234040` sont à `0x82007a30` et
`0x82009150`. Le slot `+0x6c` de l'address-point initial `0x8205d054` pointe
vers `0x82266390`, et non vers `0x82234040`.

La relation suivante est donc établie :

```text
owner statique 0x823f9b28
  -> objet 0x823f9b2c
  -> vtable initiale 0x8205d054
  -> slot +0x6c : 0x82266390

tables statiques apparentées du cycle 183
  -> 0x82007a30 / 0x82009150
  -> entrée 0x82234040
```

Ces deux familles ne doivent pas être fusionnées. Un constructeur ou une
initialisation runtime peut remplacer le vptr, mais aucune telle transition
n'est démontrée par cette passe.

## Qualification

- `confirmed` : la chaîne globale `0x823f9b28 -> 0x823f9b2c`.
- `confirmed` : le premier mot statique `0x823f9b2c -> 0x8205d054`.
- `confirmed` : la cible initiale du slot `+0x6c` est `0x82266390`.
- `confirmed` : le dispatch `0x820f8e54..0x820f8e68` suit ce contrat.
- `cross-match` : les tables `0x82007a30` et `0x82009150` restent des tables
  de pointeurs apparentées, mais elles ne sont pas la vtable initiale de ce
  global owner.
- `unknown` : nature de `0x82266390`, éventuelle réécriture du vptr, et
  relation runtime entre cet owner et le receiver NDXR.
- `needs-dynamic-evidence` : vtable effective après initialisation et rôle
  métier du payload.

`FindGlobalPointerFieldStores.java 0x823f9b28 0` ne produit aucun candidat de
store localement suivi dans les fonctions qui référencent ce global; cela ne
constitue pas une preuve d'absence de toute écriture indirecte.

## Preuves exécutées

```text
DumpDataWords.java 0x823f9af0 80
ReferencesTo.java 0x823f9b28
FindGlobalPointerFieldStores.java 0x823f9b28 0
DumpDataWords.java 0x8205d020 56
DumpDataWords.java 0x823f9b20 16
DumpRange.java 0x820f8d80 0x820f8f30
```


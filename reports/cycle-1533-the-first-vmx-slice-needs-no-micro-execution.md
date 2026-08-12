# Cycle 1533 — la première tranche VMX ne requiert aucune micro-exécution

## Résultat

La copie de locator de `0x822A6710` est portée comme un transfert opaque de
`0x40` octets. `RetailLocatorPayload` est trivial, aligné sur 16 octets et ne
convertit jamais les mots en `float` : zéros signés, sous-normaux, infinis et
payloads NaN/sNaN sont préservés bit à bit.

Cette tranche applique la politique statique d'abord. Les octets retail ne
contiennent aucune arithmétique vectorielle et une copie mémoire exacte épuise
donc la sémantique observable ; une micro-exécution n'ajouterait aucune
distinction.

## Qualification canonique

Cible : projet `ghidra-projects/ace-combat-6`, module PAL `default.xex`,
SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
La `.pdata` borne le corps de 42 instructions à
`0x822A6710..0x822A67B7`. Le projet Xenon scratch sert uniquement au décodage
des mots VMX128 suivants :

```text
822A677C 100050C3  load  child+70   822A6788 100059C3  store player+90
822A6780 11AA28C3  load  child+80   822A678C 11AB29C3  store player+A0
822A6784 118A30C3  load  child+90   822A6790 118B31C3  store player+B0
822A6794 100838C3  load  child+A0   822A6798 100939C3  store player+C0
```

Le préambule fixe ces huit adresses effectives ; aucune instruction
arithmétique ne sépare les chargements et les stores.

## Corpus quinze missions

Sur le cache v2 qualifié
`cfca517e3f843169ca01fc52700472e66b86365621a922fc27a64a21ab713f85`,
chacun des payloads 9 à 23 contient exactement un record classe 0, toujours
index 0, avec un Obj présent et son ObjBin. La chaîne statique passe par
`0x820A8138`, `0x8222BEC8`, `0x822FD2C8` et `0x8229A470`, qui construit le
locator enfant ; la vtable joueur `0x820568D4` publie `0x822A6710` au slot
`+0x3C`. Le bitmap d'éligibilité authorée est donc `0x7FFF` pour M01–M15.

Cette preuve ne prétend pas garantir la réussite d'une allocation ni la
liveness de chaque mission à l'exécution.

## Contrôles natifs

Le test utilise seize motifs entiers incluant ±0, ±infini, qNaN/sNaN avec
payloads, sous-normaux et poisons. Il compare les 64 octets par `memcmp`, garde
des canaris et des mots de type avant/après, vérifie l'auto-copie et l'absence
d'alias après mutation de la source.

```text
build ac6-retail-locator-payload-tests                    pass
CTest ac6-retail-locator-payload                          1/1
Clang ASan/UBSan et GCC strict -Werror                    pass
micro-exécution                                           aucune
```

Sur le build x86_64 courant, le compilateur abaisse le `memcpy` en quatre
paires SSE `movdqu/movups`. Ce codegen est une optimisation observée, pas une
condition de correction du port.

## Frontières restantes

La lane VMX/VMX128 reste ouverte : son census atteignable global, les
estimations réciproques/racines, les arrondis, les NaN arithmétiques et les
autres familles vectorielles ne sont pas encore scellés.

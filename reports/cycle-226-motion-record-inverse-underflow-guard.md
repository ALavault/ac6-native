# AC6 — garde de sous-débordement de l'inverse `0x8211bb80` (cycle 226)

Date : 2026-07-18.

## Identité et portée

- Cible : `ac6-xbox360-pal`
- Module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Fonction retail : `0x8211bb80`, inverse du record `0x8181`

Le modèle hôte inverse des pointeurs invités absolus vers des offsets relatifs.
Son contrat annonçait une prévalidation de tous les slots, mais une entrée
malformée pouvait placer un enfant, une sous-table ou un pointeur de sous-entrée
avant sa base, puis sous-déborder une soustraction `uint32_t`.

La tranche ajoute les préconditions d'ordre nécessaires avant chaque
soustraction : enfant `>= record`, sous-table `>= enfant`, et pointeurs
optionnels/de sous-entrée `>= enfant`. Un échec retourne `nullopt` avant toute
publication de mutation.

## Régression

Le test forge un record matériellement cohérent sauf pour son pointeur enfant,
placé sous la base du record. Le résultat doit être rejeté et la plage invitée
doit rester bit-à-bit inchangée. Les cas valides de dématérialisation restent
couverts par la même suite.

Cette garde est une sécurité du wrapper hôte; elle ne décrit pas la réponse
retail à une mémoire XEX invalide et n'ajoute aucun type avion, caméra, mission
ou vol.

## Validation

```text
cmake --build .build/ace-combat-6 -j16 --target ac6-motion-record-tests
ctest --test-dir .build/ace-combat-6 --output-on-failure \
  -R '^ac6-motion-record-tests$'
1/1 PASS

cmake --build .build/ace-combat-6 -j16
ctest --test-dir .build/ace-combat-6 --output-on-failure -j16
42/42 PASS

cmake --install .build/ace-combat-6 --prefix "$PWD"
test ! -e bin/bin
git diff --check
```

Aucun Xenia, GPU, GUI, VNC, asset retail ou geste humain n'a été utilisé.

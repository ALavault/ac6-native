# AC6 cycle 219 — lecteur encodé type `0x0011`

## Identité et preuves

- Cible : `ac6-xbox360-pal`
- Module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Projet Ghidra canonique : `ghidra-projects/ace-combat-6`
- Fonctions retail : `0x82339508`, `0x823456b0`, consommateur
  `0x8211bd50`

L'export headless montre que `0x82339508` renvoie zéro pour un record nul ou
dont le tag demi-mot à `+0x0a` n'est pas `0x0011`; sinon, il délègue à
`0x823456b0`. Cette dernière lit le mot 32 bits big-endian à
`record + (index + 8) * 4`, effectue un décalage logique à droite d'un bit,
puis fixe le marqueur 64 bits `0xffffffff80000000`. Le bras `0x0011` de
`0x8211bd50` est un appel direct à ce lecteur.

## Implémentation native bornée

`function_82339508_type_0011_encoded_word` matérialise ce contrat dans
`reconstruction/ace-combat-6`. L'adaptateur
`function_8211bd50_type_0011_entry_word` délègue directement, sans inventer
de sémantique pour la valeur encodée.

Le modèle hôte retourne `nullopt` lorsque l'adresse, l'index ou le mot invité
ne sont pas disponibles; ce garde-fou ne prétend pas reproduire le traitement
retail d'une mémoire XEX invalide. Les calculs d'adresse sont contrôlés contre
les dépassements avant toute lecture.

## Validation

La régression `ac6-motion-record-tests` couvre :

- la valeur encodée `0x00002468 -> 0xffffffff80001234` ;
- le forwarding exact de `0x8211bd50` ;
- le tag non-`0x0011` et le record nul, qui renvoient zéro ;
- un buffer invité tronqué, qui est rejeté sans lecture hors plage.

Exécuté :

```bash
cmake --build .build/ace-combat-6/native -j16
ctest --test-dir .build/ace-combat-6/native --output-on-failure -j16
cmake --install .build/ace-combat-6/native --prefix "$PWD"
test ! -e bin/bin
git diff --check
```

Résultat : AC6 **42/42 PASS**. Aucun lancement Xenia, GPU, GUI, VNC, asset
retail ou intervention humaine n'a été nécessaire.

## Frontière

Le marqueur 64 bits et les entrées lues restent des données encodées : ce
cycle n'identifie ni leur type concret, ni leur consommateur mission/vol, ni
un comportement rendu. Ces points restent `needs-types` ou
`needs-dynamic-evidence`.

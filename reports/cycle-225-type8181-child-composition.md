# AC6 cycle 225 — composition enfant du matérialiseur `0x8181`

## Identité et preuves

- Cible : `ac6-xbox360-pal`
- Module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Projet Ghidra canonique : `ace-combat-6`
- Fonctions retail : `0x82118a50` et `0x82119740`
- Exports headless : `exports/82118a50.json`, `exports/82119740.json`

Le corps canonique de `0x82118a50` fait, pour un record `0x8181` frais :

1. reloge `record+0x1c` depuis le mot relatif `+0x10` ;
2. reloge `entry+0x10` pour chaque entrée de stride `0x20` ;
3. appelle `0x82119740` avec cet enfant relogé ;
4. pose le bit `0x80000000` de `record+0x18` ;
5. renvoie le compteur à `record+0x14`.

Le cycle 224 avait déjà établi le contrat exact de `0x82119740`, mais le
wrapper parent ne l'appelait pas encore. Cette omission faisait de son résultat
une préparation partielle, alors que le flot retail appelle bien le helper une
fois par entrée fraîche.

## Implémentation native

`materialize_function_82118a50_type_8181` compose désormais le normaliseur
`normalize_function_82119740_type_1` immédiatement après chaque relogement
d'entrée, dans le même ordre que le corps PPC.

Le modèle hôte exécute ce flot sur une copie de la plage invitée, puis publie
la copie seulement si chaque entrée et chaque helper enfant réussissent. Cette
atomicité est une protection hôte pour une plage tronquée : elle ne prétend pas
décrire la gestion retail d'une mémoire XEX invalide. Un record déjà
matérialisé ne répète pas les mutations enfants.

Les mots de dispatch sélectionnés par `0x82119740` restent des adresses
invitées brutes. Cette composition ne donne aucun nom d'objet, de mission,
d'avion, de caméra ou de comportement de vol aux enfants.

## Validation

La régression `ac6-motion-record-tests` couvre désormais :

- le relogement parent et l'application effective du normaliseur enfant ;
- le non-rejeu sur un record déjà matérialisé ;
- une entrée dont le relogement parent est possible mais dont la sous-table
  enfant est tronquée : rejet complet et octets d'origine inchangés.

Exécuté :

```bash
cmake --build .build/ace-combat-6 -j16 --target ac6-motion-record-tests
ctest --test-dir .build/ace-combat-6 --output-on-failure -R '^ac6-motion-record-tests$'
cmake --build .build/ace-combat-6 -j16
ctest --test-dir .build/ace-combat-6 --output-on-failure -j16
cmake --install .build/ace-combat-6 --prefix "$PWD"
test ! -e bin/bin
git diff --check
```

Résultat : test ciblé **1/1 PASS**, corpus AC6 **42/42 PASS**, installation
racine réussie. Aucun Xenia, GPU, GUI, VNC, asset retail ou geste humain n'a
été utilisé.

## Frontière

Cette tranche ferme la composition statique du format de record `0x8181`.
L'exécution des dispatchs enfants, leurs types concrets et leur lien avec le
consommateur de mission/vol restent `needs-types` ou
`needs-dynamic-evidence`.

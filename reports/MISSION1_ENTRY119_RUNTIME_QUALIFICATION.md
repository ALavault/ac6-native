# Mission 01 — qualification runtime de DATA.TBL[119]

## Identité

Mission 1 est reliée statiquement par `0x821B7300` et la table
`0x820658D8` à l'identifiant 119. Pour les DPL inférieurs à `0x39D`, la règle
qualifiée est l'identité directe de l'entrée `DATA.TBL`.

```text
archive             DATA00.PAC
source offset       0x0C928000
stored size         0x06EE1766
decoded size        0x09E35000
stored SHA-256       c33dc3d9abd45293f3a1635534a7de099f84d7946d23d61e846dfa625bc1d142
```

Le XEX PAL et `DATA.TBL` sont ceux du corpus canonique. Aucun PAC ni payload
complet n'est ajouté au dépôt.

## Run courant

Le run `cycle-1027-entry119-selector-current` est `lane=bridge` et a utilisé le
binaire `9e0ef14cc07a6c1c1b584b73b30d989683a791790f15f75fb58f71cfe2e8bdd4`.
Le parcours UI s'est arrêté à l'étape 73 faute de fenêtre focalisable, avant la
qualification route/HUD; il est néanmoins arrivé au décodeur et a émis :

```text
entry=119 mode=1 csize=0x6ee1766 usize=0x9e35000
src_off=0xc928000 record=119 stream_slot=0 output=0xA6040000
out_head=46484d20...
```

Le buffer runtime borné fait 165 892 096 octets. Son SHA-256 est
`e57cbeeb8f97a7a607ee1315b11a822b6af2d32581dcb7cbd557f1a6280e6dbd`.

## Comparaison hors ligne

L'extraction indépendante locale du même tuple possède exactement le même
SHA-256, la même taille et la même racine `FHM `. Le parseur compte 7 FHM, 192
NTXR, 178 NDXR et 588 nœuds récursifs; l'arbre FHM normalisé est
`313401e2c17ab2dfb06e8ead3f6405076da877e7ad6d36d86248273c4f108055`.

La comparaison machine-readable est dans
`analysis/assets/entry119_runtime_offline_comparison.json`.

| gate | résultat |
| --- | --- |
| tuple source/stocké/décodé | `proven` |
| ressource chargée par le décodeur courant | `proven` |
| bytes runtime = extraction hors ligne | `qualified_equal` |
| ressource enregistrée dans le registre consommateur du run courant | `open` |
| consommateur gameplay entry119 atteint dans le run courant | `open` |
| preuve historique de join `entry119/022_FHM` | `strongly_supported`, lane bridge |

## Conclusion

La branche D (« DATA.TBL[119] mal décodé ») est rejetée pour le buffer
qualifié. Cette preuve ne suffit pas à promouvoir E (« correct mais non
enregistré/non consommé ») en cause racine : le registre et le consommateur
doivent encore être joints dans un run gameplay courant. Elle ne réhabilite pas
le sélecteur 2D, qui est déjà observé `full_3d` en C5/C6.

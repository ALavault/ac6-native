# AC6 cycle 222 — dématérialisation type `0x8181`

## Identité et preuves

- Cible : `ac6-xbox360-pal`
- Module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Export headless : `exports/8211bb80.json`
- Fonction retail : `0x8211bb80`

Sur un record `0x8181` matérialisé, la fonction parcourt le tableau absolu à
`record+0x1c`. Chaque entrée de stride `0x20` retrouve son enfant à `+0x10`
et le remet en offset relatif au record. Pour les enfants de kind `1`, elle
efface `+0x34`, remet les deux pointeurs de chaque sous-entrée de stride
`0x30` en offsets relatifs, reloge la table enfant et son pointeur optionnel,
puis efface le bit `0x80000000` du record et restaure le champ `+0x1c`.

## Implémentation native bornée

`denormalize_function_8211bb80_type_8181` prévalide chaque adresse de table,
enfant et sous-entrée avant la première écriture. Les transformations sont
des soustractions 32 bits invitées ; les valeurs produites ne sont jamais
déréférencées comme pointeurs hôte. Une représentation non matérialisée est
un no-op borné.

Le résultat expose seulement le nombre d'entrées et le nombre d'enfants kind
`1`. Il n'attribue aucun nom au sous-objet et ne prétend pas représenter un
avion, une mission ou un état de vol.

## Validation

La régression `ac6-motion-record-tests` couvre une entrée, une sous-entrée,
les deux pointeurs, le champ optionnel, le bookkeeping, le flag et le second
appel sans effet.

```bash
cmake --build .build/ace-combat-6/native -j16 --target ac6-motion-record-tests
ctest --test-dir .build/ace-combat-6/native --output-on-failure \
  -R '^ac6-motion-record-tests$'
cmake --build .build/ace-combat-6/native -j16
ctest --test-dir .build/ace-combat-6/native --output-on-failure -j16
cmake --install .build/ace-combat-6/native --prefix "$PWD"
test ! -e bin/bin
git diff --check
```

Résultat : test ciblé **1/1 PASS**, corpus AC6 **42/42 PASS**. Aucun Xenia,
GPU, GUI, VNC ou geste humain n'a été utilisé.

## Frontière

Le contrat inverse la représentation mémoire seulement. Les types des
sous-objets, leurs producteurs et leurs consommateurs de vol restent
`needs-types` / `needs-dynamic-evidence`.

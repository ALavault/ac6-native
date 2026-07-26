# Cycle 176 — fan-out statique du receiver partagé avant le slot `+0x140`

Date : 2026-07-18 (Europe/Paris)

## Identité de la cible

- target ID : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- projet Ghidra : `ace-combat-6`
- base : `0x82000000`
- langage : `PowerPC:BE:64:A2ALT-32addr`

Cette passe est headless, statique et en lecture seule. Aucun run Xenia, VNC ou
intervention humaine n'est requis.

## Initialisation de la table

Le chemin `0x8226a8b0..0x8226a9b0` recharge le global/table
`PTR_DAT_826e4eb4` et installe plusieurs alias par `stwx`. Le lien utile pour
le receiver étudié est :

```text
table[0x36084] = table + 0x3607c
```

Le même bloc initialise notamment les voisins `table+0x36078` et
`table+0x360a8` à partir de zones de la même table. Il s'agit d'adresses de
cellules runtime, pas de données statiques interprétables comme une vtable.

## Test d'identité du receiver

La fonction `FUN_82268468` se décompile en :

```c
bool FUN_82268468(int param_1)
{
    return *(int *)(param_1 + 0x2b8) !=
           *(int *)(PTR_DAT_826e4eb4 + 0x36084);
}
```

Ce résultat confirme un test de pointeur contre le singleton publié dans
`table+0x36084`. Il ne fournit pas de nom de classe ni de sémantique gameplay.

## Dispatchs partageant le même champ

Des dumps bornés montrent plusieurs chemins qui chargent le même pointeur via
`table+0x36084`, puis lisent le premier mot comme vtable/address-point :

| Site | Slot lu | Arguments observables |
|---|---:|---|
| `0x82228de8..0x82228e04` | `+0x04` | `r4 = r31+0x50`, `r5 = 1` |
| `0x822334c8..0x822334e4` | `+0x04` | `r4 = r1+0x60`, `r5 = 1` |
| `0x82233824..0x82233840` | `+0x04` | `r4 = r30`, `r5 = 1` |
| `0x8231c684..0x8231c6a0` | `+0x04` | `r4 = r28+0x50`, `r5 = 1` |
| `0x8226bc04..0x8226bc1c` | `+0xc8` | `f1 = f31`, puis appel indirect |
| `0x82213558` dans `sub_822131d0` | `+0x140` | sorties locales, records `r7/r8`, `r9 = 1` |

La convergence prouve une frontière de receiver commune et plusieurs méthodes
virtuelles. Elle ne prouve pas que la vtable effective du singleton est le
bloc statique `0x8205c9a4`.

## Relation avec la table candidate

Dans le bloc statique candidat `0x8205c9a4`, les mots correspondants sont :

```text
0x8205ca?  (+0x04)  -> 0x82101bf0
0x8205ca6c (+0xc8)  -> 0x821005c8
0x8205cae4 (+0x140) -> 0x82102e70
```

Ces concordances de slots renforcent le classement `cross-match` déjà établi,
mais le premier mot du receiver `table+0x3607c` réside dans la BSS et n'est pas
résolu statiquement. L'installation runtime de la vtable reste donc
`needs-dynamic-evidence`.

## Décision

- `confirmed` : le global/table, l'alias `table+0x36084`, le test d'identité et
  les dispatchs statiques aux slots indiqués.
- `cross-match` : correspondance des slots avec `0x8205c9a4` et le parent
  `0x82102e70`.
- `unknown` : nom de classe, type du receiver, rôle métier et vtable effective
  observée à l'exécution.
- Ne pas attribuer de sémantique aux records ou aux floats sur la seule base de
  ce fan-out.

## Commandes et sources

```text
DumpRange.java 0x8226a6e0 0x8226a9c8
DumpRange.java 0x82268440 0x822684b0
DumpRange.java 0x8226bbd0 0x8226bc40
DumpRange.java 0x82228db0 0x82228e20
DumpRange.java 0x82233490 0x82233540
DumpRange.java 0x822337f0 0x82233880
DumpRange.java 0x8231c640 0x8231c710
DecompileAt.java 0x82268468
DumpDataWords.java 0x8205c9a4 0x60
```

La prochaine précision possible est une capture runtime ciblée du premier mot
de `table+0x3607c` au moment du dispatch. Elle reste optionnelle et ne bloque
pas la progression statique AC6.

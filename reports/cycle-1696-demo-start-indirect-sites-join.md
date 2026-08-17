# Cycle 1696 — jointure RTTI des trois arêtes START du tick 268

## Résultat

Les deux arêtes apparues après le shim du cycle 1694 sont maintenant
qualifiées par lecture guest et atlas RTTI PAL :

- `LR 0x82321F30 -> 0x820D0D10` : objet `0x2E3CF9D0`, vtable
  `0x820064D8`, slot 11;
- `LR 0x823231B8 -> 0x820DEA08` : trois objets distincts
  (`0x2E3DE8D4`, `0x2E3DF154`, `0x2E3DE154`), vtable `0x82006D8C`, slot 3;
- `LR 0x82321F34 -> 0x820E7E08` : objet `0x2E3D3D14`, vtable
  `0x820077AC`, slot 78.

Les trois sélections sont uniques et leurs bytes/atlasses sont PAL démo. Un
A/B process/store frais au même binaire atteint 300 ticks et 163 PRESENT sur
les deux routes, sans frontend, mission, terminal ni trap. Cette jointure ne
donne pas encore la sémantique des classes ni un état visuel.

## Identité et A/B

| élément | valeur |
|---|---|
| cible | `Default.xex` PAL démo, SHA-256 `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| architecture | Xenon big-endian / Xenos |
| basefile | SHA-256 `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| binaire | `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp`, SHA `d76dbe00eccb4f7935b38e75c59d71d3d63c38778ffd5f39c321a742843ed355` |
| source dispatch | `src/guest_bridge.cpp`, SHA `712c34daa408d80a85587dd6bc02b82a4d66464148f16396ae5e7c688ded3e65` |
| hook | `AC6_DEMO_WATCH_INDIRECT_OBJECT=1`, désactivé par défaut |

| route | RTPLY SHA | rapport SHA | stderr SHA | résultat |
|---|---|---|---|---|
| neutral, 300 ticks | `2e49ae679129c17283c39855ba662c17a8eaba8349e622f88e01eca50ad9c115` | `c931a747e1cabf2aac8e7eee426b08f5033c74e327480064882e5b3ed7cd9df1` | `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` | max_ticks, 163 PRESENT |
| START tenu 252–267, 300 ticks | `3613f8e02c97bd0f91d7889172c4507e56aaa8953b79ad09a13996592d0aaa47` | `f57d77c65f81b2d03021ce3734b89518e1253471bb78f825167db4a7f42e08eb` | `1cde617bc5b5253e4515f3ace8b83d81a001b59bb3cffd4d50fca4ecb4cc3a0e` | max_ticks, 163 PRESENT |

Le rapport START contient les trois champs `virtual_dispatch` exacts; le
rapport neutral n’atteint aucune de ces arêtes.

## Table qualifiée

| LR | cible | vtable | slot | objets observés | bytes/atlas |
|---|---|---|---:|---|---|
| `0x82321F30` | `0x820D0D10` | `0x820064D8` | 11 | `0x2E3CF9D0` | chunk `0x820D0D10..0x820D0D7B`, bytes SHA `4fffdf821110b8666232792686171c50b90498ed759cf76c6a20fcae854bea1b` |
| `0x82321F34` | `0x820E7E08` | `0x820077AC` | 78 | `0x2E3D3D14` | chunk `0x820E7E00..0x820E7E0F`, bytes SHA `988a4167846e4f600a58f19d51a7e694a854985677cad48f0e5d73f1efda9180` |
| `0x823231B8` | `0x820DEA08` | `0x82006D8C` | 3 | `0x2E3DE8D4`, `0x2E3DF154`, `0x2E3DE154` | `.pdata` `0x820DEA08..0x820DEAA3`, bytes SHA `a7db8c3da7a49d593c1595910dcef2b882340e4df4f27f11350cca1e85a6c9d4` |

La qualification ajoute uniquement ces sites et leurs slots au garde de
dispatch; elle n’édite ni le codegen, ni le projet Ghidra, ni les microcodes.

## Classification et suite

- `demo-qualified` : objets, vtables, slots, cibles, A/B frais et absence de
  trap à 300 ticks.
- `demo-observed` : trois arêtes START, leurs compteurs et la cadence 163
  PRESENT commune.
- `xenia-generic` : aucun élément.
- `unknown` : ABI métier et effets des trois callees, writer/consumer d’état
  frontend, pixels, audio, mission et terminal.

Prochain checkpoint : capturer les stores/loads hors pile autour de ces trois
sites et leurs retours; l’égalité des PRESENT ne vaut pas preuve de transition
visuelle. Le fail-closed et l’absence de fallback restent obligatoires.


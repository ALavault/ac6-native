# AC6 retail NTSC-U/J — Gate 0 état canonique

Date: 2026-08-30.

## Décision

La feuille de route active revient au produit
`recompilation/ace-combat-6-retail`. Le gate N2 de
`reconstruction/ace-combat-6` est abandonné pour cette feuille; ses sources et
preuves restent intactes, historiques et non fusionnées. Aucun runtime N2,
PAL, campagne ou save/reload n'est lancé dans cette transition.

## Identités scellées

| Élément | Valeur qualifiée |
| --- | --- |
| target | `ntsc-uj`, retail US/Japan |
| XEX | `default.xex`, SHA-256 `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc` |
| ISO | SHA-256 `204c5e645d79da8776699c12f17bd069f869fbdb10ae79015d1a0ef2b743c98c` |
| Ghidra | `ghidra-projects/ac6-us/default.xex`, `PowerPC:BE:64:Xenon` |
| AC6_recomp | `09144bb092ad871584808aeead69c395edbd5200` |
| SDK tree | `abb22fd981596dae441af88eb25b43bbe27a0c8c`, version `0.8.0` |
| route | `routes/mission01-qualified-96.steps`, SHA-256 `771a77a8ff50eda30c5fb24309d8828bb339f91a49471368f117b65c9dbb6043` |
| renderer oracle | ReXGlue Xenos/Vulkan, read-only only |

Ces valeurs recoupent `targets/ntsc-uj.json` et le manifeste retail existant.
Les octets XEX/ISO restent externes; aucun média n'est copié dans le dépôt.

## Outillage

Les checkouts ignorés ont été restaurés et détachés aux révisions qualifiées:

- XenonRecomp `ddd128bcca99fe8bfbb99bea583c972351fa6ace`;
- XenosRecomp `990d03b28a27b50277ee5d8d942e1c5f873869d1`.

Les deux arbres et tous leurs submodules verrouillés ont `git status
--porcelain` vide. Provenance et chemins relatifs au portfolio sont
scellés dans `recompilation/ace-combat-6-retail/config/xbox360-toolchain.lock.json`.
Le catalogue local `.tools/knowledge-base/architecture-v1/catalog.json` est
absent de cet environnement; aucune assertion générique Xenon/Xenos n'est
déduite de cette absence.

## Validation Gate 0

- `pytest -q recompilation/ace-combat-6-retail/tests`: `94 passed`.
- `tools/validate_toolchain.py`: deux checkouts épinglés propres; catalogue
  architecture absent explicitement signalé.
- identité route: `sha256sum` égale à la valeur scellée ci-dessus;
- sous-modules/outils: révisions et arbres propres vérifiés;
- changements existants préservés, y compris le checkout AC6_recomp modifié
  par l'oracle et la cible `reconstruction/ace-combat-6`.

Gate 0 est fermé vert pour identité et outillage. Gate 1 reste ouvert:
aucune capsule Xenos, renderer natif, preuve PM4 ou runtime sans ReXGlue n'est
encore revendiqué.

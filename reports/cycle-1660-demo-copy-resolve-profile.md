# Cycle 1660 — profil copy/resolve PAL

## Résultat

Le draw copy de l’IB PAL est maintenant relié à un profil de registres exact
et à l’ordre PM4 autour du `XE_SWAP`. Le source est le color RT0 en mode copy;
la destination et sa géométrie sont entièrement bornées. Cela ne prouve pas
encore le contenu EDRAM ni les pixels produits.

| registre | brut | décodage générique | qualification |
|---|---:|---|---|
| `RB_SURFACE_INFO` | `0x14000500` | pitch 1280, MSAA 1× | demo-qualified |
| `RB_COLOR0_INFO` | `0x00000000` | base 0, format 0 (`8_8_8_8`) | demo-qualified |
| `RB_MODECONTROL` | `0x00000006` | copy | demo-qualified |
| `RB_COPY_CONTROL` | `0x00100000` | RT0, sample 0, convert, aucun clear | demo-qualified |
| `RB_COPY_DEST_BASE` | `0x1374A000` | base destination | demo-qualified |
| `RB_COPY_DEST_PITCH` | `0x02D00500` | pitch 1280, hauteur 720 | demo-qualified |
| `RB_COPY_DEST_INFO` | `0x01000300` | format 6, endian 0, swap 1 | demo-qualified |

## Ordre causal borné

`copy regs@326 → VS copy@333 → mode copy@354 → draw@387 → event@389 →
cohérence base/size@395 → wait@398 → reset copy@404/406 → fetch@408 →
XE_SWAP@415`.

Le reset après le wait interdit d’interpréter les valeurs postérieures comme
un second copy. La fenêtre de cohérence `0x00385000` est conservée comme
observation cache et n’est pas utilisée comme borne d’allocation ou de
readback.

## Limite restante

Le champ bloquant n’est plus l’adresse, le pitch, le format, l’endian ou le
tiling de la destination. Il reste le contenu exact du color RT0 dans l’EDRAM
avant le draw copy, puis l’exécution réelle du copy/resolve et son readback
guest. Le chemin Vulkan actuel reste test-only sur un EDRAM synthétique.

Capsule durable : `analysis/demo/ac6-demo-copy-resolve-profile-v1.json`.

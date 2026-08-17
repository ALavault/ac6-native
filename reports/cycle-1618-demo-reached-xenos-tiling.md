# Cycle 1618 — tiling Xenos exact du resolve atteint

## Résultat

Le chemin générique `texture_util::GetTiledOffset2D` de ReXGlue
`cb58065c793429aa92895d778af58d12e9d26d8f` est adapté dans une routine bornée
à l'unique configuration démo atteinte : format texture brut 6, RGBA8 32 bpp,
pitch 1280, 1280×720, endian 0, destination tiled.

Pour le rectangle complet `(0,0)-(1280,720)` :

- offset `(0,0)` : `0x0` ;
- offset `(1279,719)` : `0x39777C` ;
- origine du dernier tile : `0x397000` ;
- borne supérieure Xenos : `0x398000` bytes ;
- plage destination calculée : `[0x1374A000,0x13AE2000)`.

Le receipt durable
`analysis/demo/ac6-demo-reached-resolve-tiling-v1.json` sépare les registres et
la géométrie `demo-qualified`, la formule `xenia-rexglue-generic`, les valeurs
calculées et les pixels toujours `unknown`.

## Garde importante

`COHER_BASE_HOST=0x1374A000` et `COHER_SIZE_HOST=0x00385000` sont observés après
le draw copy. Cette fenêtre de cohérence est `0x13000` bytes plus courte que
l'étendue tiled calculée. Xenia ne la traite que comme une requête de cohérence
cache et indique encore un TODO pour notifier les caches ; elle ne constitue
donc ni une taille d'allocation ni une borne de readback. Le runtime ne la
réinterprète pas et aucun pixel n'est lu sur cette base.

## Tests

`ac6-demo-xenos-tiling-tests` vérifie six offsets aux frontières, l'unicité et
la borne des 921 600 adresses pixel, un motif synthétique tile→untile et le trap
hors bornes. La routine d'untile exige exactement `0x398000` bytes tiled et
3 686 400 bytes linéaires ; elle n'est pas appelée par `play`.

Validation : build incrémental ccache, CTest headless 16/16, Python 27/27,
audit source, complexité et `git diff --check` passent.

## Frontier

Le calcul d'adresse/pitch/format/endian/tiling est fermé. Le prochain champ
bloquant reste le contenu exact de l'EDRAM RT0 au draw copy : effets ordonnés
des 24 point draws bootstrap, état complet du draw normal/copy, conversion
RT format brut 0 vers destination format 6 et effet prouvé du swap de canaux.
Sans ces valeurs, aucun readback neutral ni screencap guest ne peut être publié.

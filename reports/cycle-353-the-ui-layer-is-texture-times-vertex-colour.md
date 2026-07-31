# Cycle 353 — la couche d'interface est « texture × couleur de sommet », et c'est le produit qui s'annule

## 1. Les deux shaders, côte à côte

Vidés avec `--dump_shaders`. Le shader **défaillant** tient en six instructions :

```
exec
   tfetch2D r0, r0.xy, tf0      ; échantillonne la texture avec l'interpolateur 0
alloc colors
exece
   mul oC0, r0, r1              ; sortie = texture x r1
cnop
```

`r1` **n'est jamais écrit** dans ce shader : c'est un **interpolateur** reçu du
vertex shader.

Son vertex shader apparié, `472913F460D4B446` :

```
vfetch_full r2.zyxw, r0.x, vf0, Offset=12, DataFormat=FMT_8_8_8_8, Stride=13
...
alloc interpolators
   max o0.xy__, r1.zwww, r1.zwww   ; coordonnées de texture
   max o1,      r2,      r2        ; couleur de sommet
```

`o1` transporte une **couleur de sommet empaquetée** (`FMT_8_8_8_8`, dernier
mot d'un sommet de 13 mots), et c'est elle qui devient `r1` côté pixel.

**La couche d'interface entière est donc « texture × couleur de sommet ».**

## 2. Pourquoi cela explique tout ce qui a été mesuré

Si cet attribut `FMT_8_8_8_8` décode à zéro, le produit est nul : sortie
entièrement transparente. La géométrie s'exécute, la texture est correctement
chargée, l'état de rendu est sain — et rien n'apparaît. C'est **exactement** le
tableau accumulé depuis le cycle 343.

Et le témoin s'explique du même coup. Le shader de l'écran-titre,
`1899F02DC6758D8F`, écrit `oC0` depuis ses propres `tfetch2D` et les constantes
`c254`/`c255` :

```
add  r1.xyz_, r1.xyzz, c254.xyyy
mul  r0,      r1.zzyx, c255
add  oC0.x0z0, r0.wwww, r0.yzzz
```

**Il ne multiplie jamais par une couleur de sommet.** D'où : il s'affiche.

C'est la première explication qui rende compte *à la fois* de la couche
invisible et du fait que l'autre passe fonctionne, sans hypothèse
supplémentaire.

## 3. Statut

**Ce n'est pas encore démontré.** Ce qui est établi est la *dépendance* :
la sortie est le produit de la texture et d'un attribut de sommet empaqueté.
Ce qui reste à mesurer est la *valeur* : cet attribut arrive-t-il à zéro ?

Deux candidats du cycle 352 se réduisent à un seul chemin concret :

| candidat | statut |
|---|---|
| décodage DXT4/5 | **affaibli** — la texture est un facteur, pas le produit ; un décodage fautif donnerait une couche visible mais fausse, pas absente |
| traduction du pixel shader | **reformulé** — le shader est simple et correct ; le suspect est l'**attribut de sommet** qu'il consomme |

## 4. Front suivant, et il est court

1. **Test décisif, sans instrumentation** : forcer `oC0 = r0` (ignorer `r1`)
   pour ce seul shader. Si l'interface apparaît, la couleur de sommet est nulle
   et la cause est le chemin `vfetch FMT_8_8_8_8`.
2. Sinon, journaliser la valeur décodée de l'attribut `FMT_8_8_8_8`
   (`Offset=12`, `Stride=13`) pour cette passe.

Les shaders vidés sont conservés dans `reports/code/` comme entrées.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

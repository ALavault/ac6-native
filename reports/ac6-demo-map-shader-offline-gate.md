# AC6 démo PAL — gate hors ligne des shaders Map/terrain

## Résultat

Le gate statique traduit et valide 72 des 78 microcodes Map/terrain uniques :

| Stade | Qualifiés | Total | Pipeline |
|---|---:|---:|---|
| Pixel | 28 | 28 | microcode exact → ReXGlue → `spirv-val` |
| Vertex | 44 | 50 | `ShaderContainer` exact → XenosRecomp → normalisation gardée → DXC → `spirv-val` |
| Total | 72 | 78 | fail-closed |

Deux exécutions fraîches, chacune repartant de l'archive vérifiée et d'une
nouvelle extraction de l'entrée PAC 163, produisent un reçu byte-identique :
SHA-256 `1aa151be…3297e`. Tous les fichiers binaires et générés restent sous
`/fastdata/lavaulta/tmp`.

## Frontière vertex clarifiée

Les 50 VS statiques ne doivent pas être traités comme les microcodes immédiats
déjà patchés du frame. Leur analyse ReXGlue brute atteint des `vfetch_full` dont
le format vaut zéro et s'arrête sur le switch Xenos générique. Les trois layouts
NSXR bornés `0`, `0x40` et `0x80` ne fournissent aucun autre candidat valide.

Ce résultat ne rend pas les shaders invalides. Le `ShaderContainer` NSXR exact
conserve la déclaration vertex et les interpolateurs séparément du microcode.
XenosRecomp consomme précisément ces champs pour résoudre les entrées HLSL. Un
premier conteneur `vsMapCstCT.updb` produit ainsi un HLSL puis un SPIR-V Vulkan
1.1 validé, sans patch ni format inventé.

Le CLI ReXGlue test-only accepte désormais `--register-count auto`. Ce mode est
autorisé uniquement si `AnalyzeUcode` déclare l'absence d'adressage dynamique ;
sinon il écrit le diagnostic, retourne 4 et ne produit aucun SPIR-V. Les 28 PS
Map utilisent tous des registres statiques.

## Alias vertex fermés sans approximation

- 8 VS uniques, 10 occurrences : le `ShaderContainer` exact contient au moins
  deux `VertexElement` d'adresses distinctes portant le même couple
  `TexCoord,1`. Ces adresses sont les adresses des instructions `vfetch`, pas
  des adresses de streams. Le corps produit par XenosRecomp lit déjà chaque
  fetch via le même `iTexCoord1`.
- Le gate exige que le nombre de paramètres HLSL répétés soit exactement celui
  des éléments du conteneur et que leurs lignes soient byte-identiques. Il
  conserve le premier paramètre et retire uniquement les répétitions. Les 10
  occurrences compilent ensuite et passent `spirv-val`.

## Six blocages sémantiquement qualifiés

- L'analyse ReXGlue exacte donne le masque `1`
  pour `VSPointSizeEdgeFlagKillVertex`, soit uniquement l'export Xenos
  `oPts.x` (taille de point). XenosRecomp ne traite que `VSPosition=62`, cherche
  donc à tort le registre spécial `63` dans les interpolateurs, puis plante.
  Aucun HLSL partiel n'est promu.

La cause est recalculée par le gate depuis le microcode exact. Elle remplace
l'ancien symptôme `SIGSEGV` sans promouvoir les six shaders.

Les variantes Map et Map_HDR produisent deux ensembles parallèles. Les 102
occurrences qui passent n'ont aucune divergence HLSL ou
SPIR-V entre conteneurs partageant le même microcode. Après compilation, zéro
SPIR-V échoue à `spirv-val`.

## Reproduction

Depuis le workspace :

```sh
export TMPDIR=/fastdata/lavaulta/tmp
python3 tools/qualify_ac6_map_shaders.py \
  demo-game-file/extracted/stfs-root \
  ac6-pal-shader-identification-20260816.tar.zst \
  recompilation/ace-combat-6-demo/build/ac6-demo-rexglue-shader-cli \
  ../../.tools/xenosrecomp-build/XenosRecomp/XenosRecomp \
  ../../.tools/dxc-install/bin/dxc \
  ../../.tools/spirv-tools-install/bin/spirv-val \
  "$TMPDIR/ac6-map-offline-gate.json"
```

Le writer refuse une collision de sortie. L'archive, les PAC, les conteneurs,
les microcodes, le désassemblage, le HLSL et le SPIR-V ne sont jamais copiés
dans le dépôt.

## Prochain test ciblé

Pour `oPts.x`, adapter un chemin test-only vers le builtin `PointSize` en
reprenant la sémantique Xenos générique, puis exiger un test de draw points avec
l'état de taille/clamp atteint. Tant que cette preuve manque, les 6 VS restent
bloqués.

Reçu : `analysis/demo/ac6-demo-map-shader-offline-gate-v1.json`.

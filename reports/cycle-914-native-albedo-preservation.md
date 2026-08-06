# Cycle 914 — préserver l’albédo natif dans le readback

Date : 2026-08-04

## Changement

Le chemin de shade CPU ne détruit plus la couleur source par XOR puis division
de luminance. Quand un PPM qualifié est lié à un `MissionTextureBinding`, ses
canaux RGB sont conservés et reçoivent seulement un gain déterministe borné
(`0.78..1.026`) dérivé des identités matériau/shader/texture. Le fallback
matériau suit la même règle. Les identités restent donc sensibles sans rendre
les surfaces artificiellement noires.

Le manifeste externe `/tmp/ac6-mission01-manifest-ppm-20260804/manifest.tsv`
produit toujours une capture 1280×720, avec le même contenu géométrique
clairsemé et une luminance maximale mesurée de 105 au lieu de 63 avant ce
changement. Cette capture n’est pas une preuve de parité : la caméra qualifiée
disponible est la lane bridge et la couverture terrain reste incomplète.

## Validation

- `SDL_AUDIODRIVER=dummy xvfb-run -a ctest --test-dir reconstruction/ace-combat-6/build --output-on-failure` : 3/3 ;
- `--present-manifest ... 1 native-color.ppm native-depth.f32` : succès ;
- hash/identité des surfaces restent testés par `product_runtime_tests` ;
- aucune archive retail n’est embarquée.

## Limite restante

Le monde reste visuellement très sombre/clairsemé car la référence oracle
positive et le raccord caméra/transforms de Mission 01 stock ne sont pas encore
fermés. Le gain de shade corrige uniquement la perte de signal couleur ; il ne
fabrique pas de terrain ou de pose synthétique.

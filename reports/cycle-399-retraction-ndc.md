# Cycle 399 — rétractation : l'échelle NDC « nulle » était un artefact de ma sonde

## 1. La lecture de code tranche contre ma propre conclusion

Le cycle 398 concluait que 87 % des tracés portent une échelle NDC nulle,
« qui prise littéralement effondre toute géométrie ». C'est **faux**, et la
cause est ma sonde.

`util/draw.cpp:440-456`, branche `pa_cl_clip_cntl.clip_disable` :

```
viewport_info_out.xy_extent[i] = min(kTexture2DCubeMaxWidthHeight, ...)   // 8192
float pixels_to_ndc_axis = 2.0f / extent_axis_unscaled_float;             // 2/8192
ndc_scale[i]  = scale_xy[i] * pixels_to_ndc_axis;
ndc_offset[i] = (offset_base_xy[i] - extent*0.5f + offset_add_xy[i]) * pixels_to_ndc_axis;
```

Avec `vport_x_scale_ena` à faux, `scale_xy` vaut 1, donc :

| grandeur | valeur réelle | imprimée en `{:.3f}` |
|---|---|---|
| ndc_scale | 0.000244140625 | **0.000** |
| ndc_offset | −1.0 | −1.000 |

La transformation est **correcte** : x_fenêtre 0 → NDC −1, x_fenêtre 8192 →
NDC +1, avec un viewport de 8192. C'est le trajet des sommets pré-transformés,
et il fonctionne.

Confirmation forte et indépendante : l'offset calculé vaut exactement −1.0, la
valeur même que le journal affiche. Les deux nombres coïncident au bit près.

## 2. Cause de l'erreur

Une chaîne de format à trois décimales sur une grandeur dont l'ordre de
grandeur utile est 10⁻⁴. Rien d'autre. La sonde est corrigée en `{:.9g}`, avec
un commentaire nommant l'erreur pour qu'elle ne se reproduise pas.

## 3. Bilan de trois rétractations

| cycle | conclusion | cause de l'erreur |
|---|---|---|
| 394 | textures manquantes | déduction jamais vérifiée par un test de suppression |
| 397 | navigateur non soumis | surimpression opaque devant la zone examinée |
| 399 | échelle NDC nulle | précision d'affichage insuffisante dans ma propre sonde |

Les trois partagent une forme : **l'instrument n'a pas été mis en cause avant
la conclusion.** La différence utile ici est que celle-ci a été trouvée en
lisant le code plutôt qu'en accumulant des mesures — c'est le moyen le moins
cher et il aurait dû venir en premier.

## 4. Ce qui reste vrai

Le débordement du panneau mesuré au cycle 397 (bord gauche 264, bord droit
jamais refermé sur 1280) est une mesure de pixels, indépendante de la sonde
NDC. Elle tient.

Ce qui tombe : l'explication proposée pour ce débordement.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

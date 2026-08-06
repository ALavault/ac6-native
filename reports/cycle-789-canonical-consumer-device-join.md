# Cycle 789 — raccord device du consommateur canonique

Date : 2026-08-04

## Résultat

Le probe oracle Midasm est installé à `0x8234D150` avec
`after_instruction=true`, puis vérifié dans le code généré et le binaire :

```text
generated/ac6recomp_recomp.38.cpp:
  ac6CanonicalAxisConsumerProbe(ctx.r3, ctx.r10)
nm:
  T ac6CanonicalAxisConsumerProbe(rex::Register&, rex::Register&)
```

La trace instrumentée observe quatre appels consécutifs :

```text
consumer #1 r3=0x8290DE3C ly_result=0x0000
consumer #2 r3=0x8290DE3C ly_result=0x0000
consumer #3 r3=0x8290DE3C ly_result=0x0000
consumer #4 r3=0x8290DE3C ly_result=0x0000
```

Le `r3` du consommateur indexé rejoint donc exactement le device canonique
qualifié par le cycle 782 (`0x8290DE3C`). Cela ferme le maillon statique +
dynamique `device → 0x8234D110 → lhzx`, au moins dans la fenêtre LY nulle.

## Limite observée

Le runtime oracle reste dans un écran noir avec overlay diagnostic après huit
présentations et n'atteint pas `type28=30` / Mission 01 dans 120 secondes.
Le driver SDL dummy est désormais bien actif (`SdlAudioDriver initialized`),
mais aucun appel instrumenté avec `ly_result != 0` n'est observé. Les runs
786–788 ont établi que ce stall n'est pas un crash ni un défaut d'activation
du driver audio. Il ne constitue donc pas une preuve de contrôle pitch positif.

## Validation

- Ghidra canonique : candidat unique `0x8234D150` dans `0x8234D110` ;
- codegen ReXGlue : succès, hook présent dans le code généré ;
- build `ac6recomp` : succès ;
- `nm` : symbole du probe présent ;
- run Xvfb avec `SDL_AUDIODRIVER=dummy` : 4 appels, `r3=0x8290DE3C` ;
- aucun changement du produit natif `ac6_product_core` / `ac6-native`.

## Prochain test discriminant

Réparer ou sélectionner le profil oracle qui atteint réellement Loading →
Game, puis rejouer les fenêtres pitch positif, nul et retour à zéro. Le contrat
G8 ne sera retenu que lorsque le même `r3=0x8290DE3C` produira une valeur LY
non nulle et une réponse mesurable de l'enfant joueur.

# Cycle 396 — arborescence de contenu provisionnée ; test non concluant

## 1. Ce qui a été fait

XUID du runtime : `0xB13EBABEBABEBABE` (`user_profile.cpp:22`). Arborescence
attendue par le gestionnaire de contenu (`content_root/xuid/title_id/type/`)
créée à côté du binaire :

```
build-rt/content/B13EBABEBABEBABE/4E4D07D1/00000001
build-rt/content/B13EBABEBABEBABE/4E4D07D1/Headers/00000001
```

## 2. Résultat

Le détecteur d'écran n'a jamais signalé l'écran de sauvegarde sur les
24 itérations. **La comparaison n'a pas eu lieu** : sans atteindre l'écran, la
présence ou l'absence du navigateur GAME DATA ne peut pas être observée.

Le test reste donc **non concluant** — ni l'hypothèse « stockage manquant » ni
son alternative ne sont départagées.

## 3. Fiabilité réelle de la navigation

Le pilotage par détecteur a atteint l'écran de sauvegarde à l'itération 7 lors
de deux exécutions (cycles 393, 394) et a échoué à la troisième. Il est donc
**nettement meilleur** que les horaires fixes — qui ont échoué cinq fois de
suite — sans être déterministe.

Amélioration évidente et non faite : boucler plus longtemps que 24 itérations,
et alterner davantage de touches, puisque le coût d'un essai supplémentaire est
de deux secondes.

## 4. État

L'arborescence de contenu est en place et le restera pour la prochaine
exécution ; il suffit de relancer le pilotage jusqu'à ce que l'écran soit
atteint, puis de comparer à la capture `drive3/SAVE-FOUND.png` (sans contenu).

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

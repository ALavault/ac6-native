# Cycle 358 — le test restreint rend un résultat anomal ; non interprété

## 1. Ce qui a été fait

Les deux diagnostics du cycle 357 ont été restreints par hachage de shader :

```c
REXCVAR_GET(ac6_force_white_texture_sample) &&
    current_shader().ucode_data_hash() == 0x8F1C48BA92C8E43Eull
REXCVAR_GET(ac6_force_white_vertex_colour) &&
    current_shader().ucode_data_hash() == 0x472913F460D4B446ull
```

Compilé, cache vidé, exécution de chauffe puis exécution de test — protocole du
cycle 356.

## 2. Résultat, et pourquoi il n'est pas interprété

```
non restreint (cycle 357) : moyenne RVB [173.2, 179.1, 173.0], 26 couleurs
restreint    (ce cycle)   : moyenne RVB [173.2, 179.1, 173.0], 26 couleurs
```

**Identiques à la décimale.** Restreindre le forçage à un seul shader ne peut
pas reproduire exactement le résultat d'un forçage global, sauf si la
restriction n'a pas pris effet.

Trois explications possibles, aucune vérifiée :

1. la restriction n'est pas appliquée à l'exécution (cache, ou hachage comparé
   au mauvais moment de la traduction) ;
2. le shader visé est aussi celui de la passe de présentation, ce que le
   cycle 347 contredit (`2E372EA28CC404B7` y était distinct) ;
3. l'exécution n'a pas rendu et l'image est un fond uniforme.

**Le résultat n'est donc pas interprété.** Le conclure serait exactement
l'erreur commise cinq fois dans cette enquête : lire une mesure dont le canal
n'est pas prouvé vivant.

## 3. Contrôle manquant, à faire en premier à la reprise

Avant toute lecture de ce test : **journaliser, dans le traducteur, quand la
restriction se déclenche** — une ligne nommant le hachage forcé. Si elle
n'apparaît jamais, la restriction est morte et le test ne dit rien. Si elle
apparaît pour la passe de présentation, l'explication 2 est la bonne.

Ce contrôle coûte une ligne et aurait dû précéder la mesure.

## 4. État

Acquis stables du cycle 356, non remis en cause : la couleur de sommet est
réfutée par test contrôlé, et l'échantillon de texture est le facteur nul par
déduction.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

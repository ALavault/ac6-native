# AC6 démo PAL — rafraîchissement versionné des payloads renderer

Date : 2026-08-19

## Résultat

Le cache Vulkan atteint n'est plus définitivement figé sur le premier corpus.
Deux payloads deviennent versionnés par SHA-256 :

1. la plage guest partagée `[0x127CA03C, 0x127CA0A8)` ;
2. les cinq buffers de constantes associés à chaque vertex shader atteint.

Un payload byte-identique est ignoré. Un payload différent n'avance sa
génération qu'après un upload complet. Une erreur d'allocation, de mapping ou
de création de descriptors laisse donc la version précédente réessayable.

## Mémoire partagée

Le profil initial reste strict : 5 chargements de shaders, 26 draws, quatre
modules traduits, deux pipelines et au plus un present. Après cette première
qualification, les compteurs cumulés ne sont plus comparés à une égalité
impossible. Le renderer exige seulement leurs minima et les invariants stables,
puis relit la plage guest à chaque batch de commandes non vide.

## Constantes

Le cache antérieur utilisait seulement l'identité du vertex shader. Toute
nouvelle valeur de constantes pour le même shader était donc silencieusement
jetée. Le nouveau chemin canonicalise les cinq payloads avec leurs longueurs,
calcule leur SHA-256 et reconstruit transactionnellement le descriptor set
lorsque ce digest change. L'ancien set n'est détruit qu'après construction du
nouveau.

## Observabilité

`VulkanSharedMemory` expose désormais :

- `shared_upload_generation()` ;
- `constant_upload_generation(shader)` ;
- `refresh_epoch()`.

L'epoch est monotone et permet au prochain changement de rendre invalides les
résultats `normal_draw_` et `neutral_resolve_` mis en cache dans le gouverneur
renderer.

## Limite restante

Cette passe ferme le gel des données, pas encore le gel de l'exécution. Le
`RuntimeRendererFrontier` conserve toujours son premier `normal_draw_` et son
premier resolve. Le prochain patch doit comparer `refresh_epoch()` à l'epoch du
résultat mis en cache, attendre l'inactivité de la queue, puis réexécuter le
couple normal-draw / copy-draw / resolve lorsque les payloads ont changé.

Supprimer simplement les `optional` serait une méthode humaine classique :
obtenir davantage d'images en renonçant à savoir à quelles données elles
correspondent. Ce patch prépare au contraire une invalidation déterministe.

## Validation

- test C++20 de la version transactionnelle : PASS ;
- warnings `-Wall -Wextra -Wpedantic -Wconversion -Wshadow` : PASS ;
- vérification structurelle de l'intégration : PASS ;
- aucune dépendance ou donnée propriétaire ajoutée.

# AC6 cycle 303 — le `std::bad_alloc` du cycle 302 n'est pas reproductible

## Question unique

Le cycle 302 a qualifié le retrait de `0x82345250` (28/28 assertions), puis a
refusé le changement parce que ReXGlue s'est terminé sur `std::bad_alloc`. AC6
est `runtime_blocked` depuis le 19 juillet 2026 sur cette seule observation.

La cause a été attribuée par proximité, jamais par mesure. Question de ce
cycle : le retrait de `0x82345250` provoque-t-il réellement l'échec ?

Route : `deterministic-fast-path`. Aucune sortie générée n'a été éditée, et le
clone de référence n'a pas été touché : les deux exécutions écrivent dans un
répertoire de travail jetable.

## Méthode

Deux exécutions de `rexglue codegen` avec le **même** binaire, le même XEX et
des configurations identiques à une ligne près.

- outil : `thirdparty/rexglue-sdk/out/linux-amd64/rexglue`, ReXGlue v0.8.0 ;
- XEX : `workspaces/ace-combat-6/game-files/default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
  (identique au cycle 302) ;
- base : `ac6recomp_config.toml` inchangé, 10 443 entrées `[functions]` ;
- expérience : la seule ligne `0x82345250 = { name = "rex_sub_82345250" }`
  retirée, 10 442 entrées ;
- instrumentation : `/usr/bin/time -v`, RSS crête relevée.

## Résultat mesuré

| | base (entrée présente) | expérience (entrée retirée) |
| --- | ---: | ---: |
| code de sortie | 0 | 0 |
| temps mur | 15,44 s | 15,93 s |
| **RSS crête** | **269 812 ko** | **270 076 ko** |
| `std::bad_alloc` | non | **non** |
| fonctions générées | 12 671 | 12 671 |
| unités de traduction | 52 | 52 |

Écart de RSS crête : **264 ko, soit 0,1 %**. Les deux exécutions culminent à
environ 264 Mo sur un hôte disposant de 121 Go, dont 113 Go libres.

## Effet du retrait, vérifié

- `sub_82345250` : référencé dans 2 unités en base, **0 en expérience** ;
- `sub_823450D0`, la fonction voisine, reste émise dans les deux ;
- les références à la cible de boucle `0x8234524C` passent de 11 à 9.

Le retrait fait donc exactement ce que le cycle 302 voulait, sans échec.

## Conclusion

**Le `std::bad_alloc` du cycle 302 n'est pas reproductible** et n'est pas causé
par le retrait de `0x82345250`. Il n'y a aucune pathologie mémoire : 264 Mo de
crête, à quatre ordres de grandeur de la mémoire disponible.

Le rapport du cycle 302 note lui-même que la commande fautive *« avait vidé
partiellement `generated/` »*. L'explication la plus simple est donc un échec
transitoire lié à un répertoire de sortie dans un état incohérent, et non un
défaut de configuration ni du SDK. Les deux exécutions de ce cycle écrivent
dans un répertoire neuf, ce que le cycle 302 ne faisait pas.

Ce cycle ne prouve pas que le SDK ne peut jamais épuiser la mémoire ; il prouve
que cette configuration, sur ce XEX, avec ce binaire, n'y parvient pas.

## Compilation du corpus généré

Le cycle 302 exigeait une validation au-delà de la codegen. Les deux corpus ont
donc été compilés en entier, à l'identique :

| | base | expérience |
| --- | ---: | ---: |
| unités compilées | **52 / 52** | **52 / 52** |
| échecs | 0 | 0 |

Norme requise : **C++23**. Le SDK utilise `std::byteswap` et
`std::move_only_function` ; une tentative en C++20 échoue sur les 52 unités des
**deux** variantes, ce qui est un défaut d'invocation et non une différence
entre elles.

Chaîne d'inclusion nécessaire : `rexglue-sdk/include`, `thirdparty/fmt/include`,
`thirdparty/spdlog/include`, `thirdparty/simde`.

## Limite explicite

Le retrait est accepté au niveau **codegen et compilation**, pas au niveau
runtime. Le binaire `ac6recomp` complet n'a pas été relié ni exécuté dans ce
cycle : cela reste l'action suivante avant toute revendication de
comportement.

`recompiler-generated` n'est pas `verified`.

## Commandes

```
rexglue codegen ac6.toml      # base,       exit 0, 15,44 s, RSS 269 812 ko
rexglue codegen ac6.toml      # expérience, exit 0, 15,93 s, RSS 270 076 ko
```

## Suite native AC6, inchangée

`reconstruction/ace-combat-6` reconstruit et passe **44/44**, zéro
avertissement. Cet arbre ne dépend pas de la codegen ReXGlue ; le compte sert
de référence pour confirmer qu'aucune régression collatérale n'a eu lieu.

## Action suivante

Relier et exécuter le runtime `ac6recomp` avec `0x82345250` retiré. Si le smoke
runtime passe, le retrait est pleinement accepté et le statut `runtime_blocked`
d'AC6 tombe : la preuve rassemblée ici montre déjà que le motif de blocage
enregistré au cycle 302 n'existe pas.

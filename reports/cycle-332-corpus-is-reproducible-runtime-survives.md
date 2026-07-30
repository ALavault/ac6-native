# Cycle 332 — le corpus EST reproductible ; le runtime reconstruit survit

## 0. Correction du cycle 331

Le cycle 331 concluait que le corpus `generated/` de l'arbre de référence
n'était pas reproductible depuis ses sources, et que les correctifs GapFill
perdus en étaient la cause. **C'est faux, et la vérification était à une commande.**

Mesuré :

| | arbre de référence | régénéré ici |
|---|---:|---:|
| `Unresolved branch from` | 2 071 | **2 071** |
| `Unresolved call from` | 365 | **365** |
| piège `0x823841F8 -> 0x82383EE8` | **présent** | présent |

Le corpus de référence **contient le piège exact** sur lequel le runtime
reconstruit s'arrêtait. Les profils sont identiques. Le corpus est donc
reproductible, et les correctifs GapFill n'étaient pas en cause.

Pourquoi le binaire de référence ne s'arrête-t-il pas dessus ? **Parce qu'il ne
l'atteint jamais.** Le binaire reconstruit l'atteint parce que le correctif
réseau du cycle 328 fait progresser l'invité plus loin. Rencontrer un nouveau
piège était donc un **progrès**, pas une régression — exactement le flux de
travail des cycles 306-307.

J'ai bâti tout le cycle 331 sur cette inversion, y compris sa conclusion « l'état
exact n'est pas récupérable ». Elle est retirée.

## 1. Pièges atteignables levés

Deux entrées `[functions]`, méthode des cycles 306-307 :

```toml
0x82383EE8 = { name = "rex_sub_82383EE8" }
0x82383EAC = { name = "rex_sub_82383EAC" }
```

Un premier essai avait conclu « déclarer n'aide pas ». Faux aussi : l'insertion
était tombée **hors de la section `[functions]`**. Replacée dedans, le piège
disparaît du corpus. Leçon : vérifier qu'une entrée de configuration est bien
dans la section visée avant de conclure qu'elle est sans effet.

Automatisé ensuite par `tools/ac6-clear-reachable-traps.sh`, qui boucle
exécution -> lecture du `FATAL` -> déclaration -> régénération -> reconstruction.

## 2. Résultat : le runtime survit

```text
round 1  runtime alive after 70s: no   FATAL 0x82383F98 -> 0x82383EAC  -> déclaré
round 2  runtime alive after 70s: YES  aucun FATAL  -> arrêt de la boucle
```

Le runtime reconstruit **tourne 70 s sans piège**, ce qu'aucun binaire issu de
mes régénérations n'avait fait. La chaîne complète — sources, génération,
compilation, exécution — est de nouveau fonctionnelle dans l'arbre de travail.

## 3. Ce qui ne marche pas : le correctif réseau n'agit pas depuis le SDK

Compteurs du binaire reconstruit : `eop=12`, `host_swap_presents=2`,
`wptr=0x43` — c'est-à-dire **la ligne de base d'avant le cycle 328**.

Diagnostic, mesuré :

```text
chaîne "privileged guest port" présente dans le binaire   oui
lignes de journal de remappage                            0
état du thread principal                                  __skb_wait_for_more_packets
appels bind() atteignant l'hôte (via interposeur)         1 seul
bind(fd=99 port=999)                                      -1 EACCES
```

Le code est **compilé dans le binaire** mais sa branche de repli **ne s'exécute
pas** : un seul `bind()` atteint l'hôte, donc la seconde tentative sur le port
décalé n'a jamais lieu. Le même correctif appliqué par `LD_PRELOAD` fonctionne
et donne `eop` 25-34, `presents` 8-12.

La cause de cet écart n'est pas établie. Candidats non départagés :

1. `errno` n'est pas `EACCES` au point de test dans `XSocket::Bind` ;
2. `ntohs(name->sin_port)` ne rend pas 999 sur la copie `N_XSOCKADDR_IN`, donc
   `IsPrivilegedPort` est faux ;
3. la branche est éliminée par `#if !REX_PLATFORM_WIN32`.

Départageable en une exécution par une trace inconditionnelle en tête de
`XSocket::Bind`, journalisant `ret`, `errno` et le port lu. C'est l'étape
suivante, et elle est bon marché.

## 4. Porte P0

**Non franchie.** `eop` 12, `host_swap_presents` 2 sur le binaire reconstruit ;
`eop` 34, `presents` 12 restent le meilleur résultat, obtenu au cycle 328 sur le
binaire de référence avec l'interposeur.

## 5. Règles ajoutées

1. **Vérifier qu'un artefact est irreproductible avant de le déclarer tel.** Une
   comparaison de profils de pièges — une commande — aurait évité toute la
   conclusion erronée du cycle 331.
2. **Un nouveau piège atteint après un correctif est probablement un progrès.**
   L'invité va plus loin et rencontre le défaut suivant ; le lire comme une
   régression inverse le sens de la mesure.
3. **Vérifier la section d'une entrée TOML avant de conclure qu'elle est sans
   effet.** Une insertion triée par adresse peut tomber hors de la table visée.
4. **Une chaîne présente dans un binaire ne prouve pas que son code s'exécute.**
   Ici la preuve d'exécution manquait, et c'est elle qui a montré l'écart.

`recompiler-generated` n'est pas `verified`.

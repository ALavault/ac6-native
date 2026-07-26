# AC6 cycle 304 — l'abort runtime est une branche non résolue dans `sub_82345100`

## Question unique

Le cycle 303 a montré que le `std::bad_alloc` du cycle 302 n'existe pas et que
le retrait de `0x82345250` passe codegen, compilation et édition de liens. Il
restait le smoke runtime. Question : le runtime démarre-t-il, et sinon,
pourquoi ?

## Reproductibilité établie

Deux identités reproduites au bit près contre l'état enregistré par la
campagne :

- corpus généré restauré : `474355e5d7aabfe3dfad32f3e288d061` ;
- binaire runtime reconstruit :
  `63f5ca2d0c164cfb868acc20ff761ed67d09cb371f14f57c9780f24435919132`,
  **exactement** le SHA-256 du runtime cycle 301 consigné au cycle 302.

L'environnement d'expérience est donc bit-identique à celui de la campagne.

## Attribution de l'abort

| | binaire | smoke |
| --- | --- | --- |
| cycle 301 (entrée présente) | `63f5ca2d…` | `SIGABRT`, exit 134 |
| retrait `0x82345250` | `a41169ac…` | `SIGABRT`, exit 134 |

Les deux sorties sont **identiques octet pour octet**. L'abort n'est donc
**pas** causé par le retrait. Le blocage `runtime_blocked` du cycle 302 tombe
entièrement.

## Cause racine de l'abort

Correction d'une inférence antérieure : l'abort ne se produit pas dans le code
hôte. La dernière ligne de journal hôte est simplement la dernière avant
l'entrée dans le code PPC. La pile réelle est :

```
sub_821D5EF8 -> sub_82332190 -> sub_8233CC60 -> sub_8234F788
             -> sub_8234F7D0 -> sub_8233E6B0 -> sub_82345100  -> abort()
```

À `generated/ac6recomp_recomp.37.cpp:71815`, dans
`PPC_FUNC_IMPL(__imp__sub_82345100)` :

```c
// bne 0x823452a0
// ERROR: conditional branch to unknown address 0x823452A0
if (!ctx.cr0.eq) REX_FATAL("Unresolved branch from 0x823452CC to 0x823452A0");
```

`0x823452CC` appartient à la fonction déclarée en `0x823452A8` ; sa cible
`0x823452A0` appartient à celle déclarée en `0x82345260`. La branche traverse
donc une frontière de fonction, et la cible n'est pas un label adressable.

Le cycle 302 avait explicitement noté que `0x82345260`, **`0x823452A8`** et
`0x82345300` *« ne sont pas qualifiées »*. C'est précisément la coupure
`0x823452A8`, non qualifiée, qui rend la branche irrésoluble.

## Vérification, et méthode itérative établie

`sub_82345100` est **une seule fonction réelle fragmentée par quatre entrées
déclarées** : `0x82345250`, `0x82345260`, `0x823452A8`, `0x82345300`. Trois
branches arrière la traversent :

```
0x8234530C -> 0x8234524C     0x82345318 -> 0x82345244     0x82345324 -> 0x8234523C
```

Toutes les cibles sont sous `0x82345250` ; toutes les sources sont au-dessus de
`0x82345300`.

| corpus | pièges dans `sub_82345100` | pièges au total |
| --- | ---: | ---: |
| base | 4 | 4 857 |
| retrait `0x823452A8` seul | 3 | 4 850 |
| **retrait des quatre** | **0** | **4 838** |

Chaque build a été relié et exécuté. Le retrait d'une seule coupure déplace
l'abort vers le piège suivant **de la même fonction** ; le retrait des quatre
fait sortir l'exécution de `sub_82345100` entièrement.

### La frontière a avancé

Avec les quatre retraits, l'abort se produit dans une chaîne d'appels
entièrement différente :

```
sub_8233F100 -> sub_8234B608 -> sub_82349630 -> sub_82349310 -> sub_82348FC8
```

à `generated/ac6recomp_recomp.38.cpp:14216`, motif identique :

```c
// blt cr6,0x82349030
if (ctx.cr6.lt) REX_FATAL("Unresolved branch from 0x823490A0 to 0x82349030");
```

Une seule entrée, `0x82349050`, se trouve entre la cible et la source. Son
retrait ferme ce piège (total 4 838 -> 4 836), confirmant la méthode une
deuxième fois.

### Boucle mécanique, reproductible et outillée

1. exécuter sous `gdb`, relever le couple d'adresses du `REX_FATAL` atteint ;
2. trouver l'entrée `[functions]` **strictement** entre cible et source ;
3. la retirer ; régénérer ; reconstruire ; recommencer.

Chaque itération coûte environ 16 s de codegen et 4 min de build.

Les étapes 1 et 2 sont automatisées par `tools/ac6-next-split.py` :

```
python3 workspaces/ace-combat-6/tools/ac6-next-split.py --run
```

Validé de bout en bout sur le corpus cycle 301 restauré : il retrouve seul
`sub_82345100`, la branche `0x823452CC -> 0x823452A0`, la fonction propriétaire
de la cible `0x82345260`, et propose `0x823452A8` — exactement le résultat de
l'analyse manuelle. `--count` donne le total de pièges (4 857 sur ce corpus).

L'outil est en lecture seule : il ne modifie jamais la configuration et
rappelle qu'un contrat headless reste nécessaire avant promotion.

## Métrique nouvelle : 4 857 branches non résolues à l'origine

Chaque `REX_FATAL` de ce type est un abort runtime potentiel. Le compte total
est donc une mesure directe de distance à un binaire natif exécutable, et il
n'avait jamais été relevé. Il remplace avantageusement le décompte de fonctions
générées, qui ne dit rien de l'exécutabilité.

## Limites

- Le retrait de `0x823452A8` n'est **pas** qualifié par un contrat headless
  comme l'était `0x82345250` (28/28). Il est justifié ici par la résolution
  d'une branche mesurée, pas par une preuve de frontière. Cette qualification
  reste à produire avant promotion.
- Résoudre cette branche ne garantit pas que le runtime aille plus loin : 4 850
  autres pièges subsistent.
- `recompiler-generated` n'est pas `verified`.

## Environnement

Hôte : NVIDIA RTX PRO 4000 Blackwell, Vulkan 1.4.329, `llvmpipe` en repli.
Backend Vulkan **activé** (`REXGLUE_USE_VULKAN:BOOL=ON`), 13 967 chaînes
Vulkan dans le binaire lié. Le smoke tourne sous `xvfb`, sans intégration
Vulkan NVIDIA ; cela n'a pas empêché d'atteindre le code PPC.

## Action suivante

1. Qualifier `0x823452A8` par contrat headless, comme `0x82345250`.
2. Rejouer le smoke avec les deux retraits et relever le **prochain** piège
   atteint : c'est la frontière runtime suivante.
3. Suivre `4 850` comme métrique de progression à chaque cycle.

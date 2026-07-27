# AC6 cycle 310 — l'état des registres invités diverge du code traduit

Ce cycle mesure directement ce que le cycle 309 avait déduit, et le corrige.
Le blocage de démarrage n'est pas une valeur invitée aberrante : c'est un écart
entre l'état des registres au moment de l'appel et ce que la fonction traduite
a calculé.

## 1. Correction du cycle 309

Le cycle 309 concluait que `r3` à l'entrée de `sub_821D4ED0` valait
**56 490 181**, en résolvant `r3 * 152 + 0x829E64B8 = 0x826A19B0`. Le calcul
était exact ; sa prémisse ne l'était pas — il supposait que l'adresse fautive
provenait de ce calcul.

Instrumentation de `XThread::Execute`, qui journalise pour chaque fil invité son
point d'entrée et son contexte :

```
thid=6..9    start_address=0x821D4BD0   start_context=0,1,2,3   xapi=0x821F7FC8
thid=10..13  start_address=0x821D4ED0   start_context=0,1,2,3   xapi=0x821F7FC8
```

`r3` vaut donc **0, 1, 2 ou 3**. Exactement l'indice attendu pour un tableau de
structures de 152 octets. Le jeu passe des valeurs saines.

Cela explique aussi pourquoi un point d'arrêt conditionnel `r3 > 1000` à
l'entrée de `sub_821D4ED0` n'avait **jamais** été atteint : la condition était
correcte, `r3` est petit. J'avais attribué cette non-occurrence à un échec
d'évaluation de `gdb` ; c'était en réalité le résultat de la mesure.

## 2. Ce que `sub_821F7FC8` est réellement

Ce n'est pas un appelant ordinaire mais le **tremplin de démarrage de fil**
(`XapiThreadStartup`) :

```c
ctx.ctr.u64 = ctx.r30.u64;
PPC_CALL_INDIRECT_FUNC(ctx.ctr.u32);
__imp__ExTerminateThread(ctx, base);
```

Le SDK l'appelle avec `r3 = start_address` et `r4 = start_context` ; le
tremplin appelle ensuite le point d'entrée avec `r3 = start_context`. La
répartition indirecte n'est donc **pas** suspecte : elle fait son travail, et
l'hypothèse de « mauvaise répartition indirecte » du cycle 309 tombe.

## 3. Le fait central : l'état invité contredit le code traduit

Dans `sub_821D4ED0`, en analysant tout le corps de la fonction :

- `r30` est écrit **exactement une fois** : `r30 = r3 * 152 + 0x829E64A8` ;
- `r28` est écrit **exactement une fois** : `r28 = r30 + 16` ;
- les **deux** appels à `RtlEnterCriticalSection` passent `r3 = r28`.

Avec `r3 ∈ {0,1,2,3}`, `r28` ne peut valoir que :

| `r3` | `r28` |
| ---: | --- |
| 0 | `0x829E64B8` |
| 1 | `0x829E6550` |
| 2 | `0x829E65E8` |
| 3 | `0x829E6680` |

Or `RtlEnterCriticalSection` reçoit **`0x826A19B0`**, qui n'est aucune de ces
quatre valeurs et se situe 54 Mo plus bas que la base du tableau.

**L'état des registres invités au moment de l'appel ne correspond donc pas à ce
que la fonction traduite a calculé.** C'est un défaut de traduction ou de
préservation de contexte, pas une erreur de logique du jeu.

Confirmation par ailleurs : une trace posée dans `RtlEnterCriticalSection_entry`
signalant tout en-tête de type hors {0,1,2,5} ne relève **qu'une seule**
occurrence sur un démarrage complet. Tous les autres verrous sont sains, ce qui
exclut une corruption mémoire diffuse.

## 4. Hypothèse principale : les registres non volatils

`r28` et `r30` sont non volatils (r13–r31) au sens de l'ABI PowerPC.
`sub_821D4ED0` débute par `__savegprlr_27`, qui sauvegarde r27–r31 en pile ; le
symétrique `__restgprlr_27` les restaure.

Si un chemin de sortie restaure r27–r31 **avant** l'appel — épilogue atteint
prématurément, ou frontière de fonction mal placée faisant tomber l'exécution
dans l'épilogue d'une autre fonction — alors `r28` reprend la valeur de
l'appelant, arbitraire. C'est exactement le symptôme observé.

Cette hypothèse est cohérente avec l'historique : les cycles 305 à 307 ont
retiré 1 660 coupures `[functions]` qui traversaient des sauts. Aucune n'a été
qualifiée par un contrat de frontière ; elles ont été justifiées par la
résolution de sauts mesurés. Une frontière mal placée qui compile et se lie sans
erreur peut parfaitement produire ce genre d'écart.

## 5. Prochaine tranche

1. Journaliser `r28` et `r30` **au site d'appel**, via une accroche `midasm` à
   l'adresse du `bl` vers `RtlEnterCriticalSection` dans `sub_821D4ED0`.
   Si `r28` y est déjà faux, la corruption précède l'appel et l'hypothèse §4
   est confirmée ; s'il est correct, l'écart naît dans le passage d'argument.
2. Vérifier l'appariement `__savegprlr_27` / `__restgprlr_27` dans
   `sub_821D4ED0` et dans les fonctions voisines issues des retraits de
   coupures des cycles 305–307.
3. Ne pas implémenter de types d'objets noyau supplémentaires : le cycle 309 a
   déjà établi que le manque n'est pas là, et le présent cycle le confirme.

## 6. État

- Corpus : **0 `REX_FATAL`**, 48 unités, inchangé depuis le cycle 307.
- Frontière de démarrage : 1,43 s, inchangée depuis le cycle 309.
- Deux correctifs de diagnostic versionnés sous `patches/`, tous deux
  temporaires et destinés à être retirés une fois la cause corrigée.

`recompiler-generated` n'est pas `verified`.

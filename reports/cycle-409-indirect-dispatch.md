# Cycle 409 — le gestionnaire d'entrée est appelé indirectement

## 1. Chaîne d'appel remontée

| niveau | fonction | rôle |
|---|---|---|
| noyau | `XamInputGetState` | lecture de la manette |
| invité | `sub_8234D3F0` | appelant direct, scrute à chaque trame |
| invité | `sub_8234D50C` | **aiguillage** |
| au-dessus | — | **aucun appelant direct** |

## 2. L'aiguillage

Dans `sub_8234D50C`, autour de `0x8234D548` :

```
cmpwi cr6, r11, 0
blt   → 0x8234D54C   ; r11 < 0  → sub_8234D478
bne   → 0x8234D554   ; r11 ≠ 0  → ne fait rien
                     ; r11 = 0  → sub_8234D3F0
```

Trois issues selon `r11`. La voie effectivement empruntée est celle de
`r11 = 0`, puisque c'est `sub_8234D3F0` dont le compteur croît (cycle 408) ;
`sub_8234D478` n'est appelée qu'une fois, ce qui correspond à `r11 < 0` une
seule fois dans toute l'exécution.

**Une troisième issue ne fait rien du tout** (`r11 ≠ 0` et `r11 > 0`). Elle est
notable mais je ne l'accuse pas : rien ne montre pour l'instant qu'elle est
empruntée sur l'écran bloqué, et le compteur de `sub_8234D3F0` prouve au
contraire que la voie active continue d'être prise.

## 3. Le point structurel

`sub_8234D50C` n'a **aucun appelant direct** dans tout le code recompilé. Elle
figure dans la table des fonctions (`ppc_func_mapping.cpp:15103`), donc elle est
atteinte par **appel indirect** — pointeur de fonction ou table virtuelle.

C'est cohérent avec la structure attendue d'une interface : chaque écran porte
son propre gestionnaire, installé dans un champ d'objet et appelé
indirectement. Cela explique aussi pourquoi la recherche d'appelants directs ne
donne rien et pourquoi la piste doit se poursuivre sur les **données** de
l'invité plutôt que sur son graphe d'appels statique.

## 4. Ce que cela ouvre

Si le gestionnaire est un pointeur porté par l'objet d'écran, alors l'écran
bloqué peut parfaitement scruter la manette (ce qu'il fait, mesuré) tout en
n'ayant pas de suite installée pour en tirer une transition.

La vérification consiste à instrumenter l'appel indirect qui atteint
`sub_8234D50C` et à relever quel objet le porte, sur un écran qui fonctionne
puis sur celui qui est bloqué. La comparaison des deux dirait directement si le
gestionnaire diffère.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

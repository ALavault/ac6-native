# Cycle 428 — la machine à états de l'écran, et le rôle de 997

## 1. `sub_821CE8A8` décodé

```
r11 = [this+84]                    ; état
si r11 != 0 -> 0x821CE8D4          ; déjà lancé : chemin de scrutation
[this+84] = 1                      ; état 1 = démarrage
[this+88] = 0 ; [this+92..] mis à zéro (7 mots)
r5 = 512 ; r3 = [this+36] ; r4 = 1
bl sub_821F4658                    ; XamShowDeviceSelectorUI
si r3 == 997 :  [this+84] = 2 ; r3 = 1      ; 997 = 0x3E5 = ERROR_IO_PENDING
sinon        :  [this+84] = 0 ; r3 = 0      ; remise à zéro
```

## 2. Ce que cela établit

`[this+84]` est **l'état de l'écran** : 0 au repos, 1 au démarrage, 2 en attente.

L'invité **exige `997` (`ERROR_IO_PENDING`)** pour passer en attente. Notre
`xeXamDispatchHeadless` rend précisément `X_ERROR_IO_PENDING` lorsque
l'`overlapped` est non nul (cycle 417), donc cette transition **doit** avoir
lieu, et l'état passe bien à 2.

## 3. L'endroit exact qui reste

Le chemin `0x821CE8D4`, pris quand l'état n'est pas nul, est celui qui scrute la
complétion et doit faire sortir de l'état 2. **C'est là que l'écran reste.**

Tout ce qui précède est vérifié : la modale est appelée, elle rend 997, l'état
devient 2, la complétion différée s'exécute réellement et écrit `SUCCESS` dans
l'`overlapped` (cycle 418). Il ne reste qu'un maillon — la manière dont ce
chemin **constate** la complétion.

## 4. Hypothèse, explicitement non vérifiée

Si ce chemin appelle `XamGetOverlappedResult` ou lit directement un champ de
l'`overlapped`, une divergence entre ce qu'il attend et ce que nous écrivons le
laisserait indéfiniment en état 2 — ce qui correspond exactement au symptôme :
un écran qui se redessine, accepte la navigation, et ne conclut jamais.

Je l'écris comme hypothèse. Le cycle 427 vient de réfuter la précédente en un
cycle ; celle-ci se teste de la même façon, en lisant `0x821CE8D4` puis en
relevant à l'exécution la valeur de `[this+84]` sur l'écran bloqué.

## 5. Pourquoi c'est le bon endroit

C'est le premier point de toute la série où **l'état de l'écran lui-même** est
nommé, localisé en mémoire, et où ses transitions sont lisibles. Les 30 cycles
précédents cherchaient sous cette couche.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

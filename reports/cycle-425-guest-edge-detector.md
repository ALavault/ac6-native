# Cycle 425 — l'invité calcule lui-même ses fronts de boutons

## 1. `sub_8234D378` décodé

```
lhz  r11, 72(r31)        ; boutons (demi-mot) du X_INPUT_STATE
stw  r11, 28(r31)        ; boutons courants
bl   sub_8234D110 / sub_8234D1B8
r11 = [this+28]          ; courants
r10 = [this+116]         ; précédents
r9  = ~r11
r8  = r10 ^ r11          ; bits ayant changé
r11 = r8 & r11  -> [this+20]   ; fronts APPUYÉS
r10 = r8 & r9   -> [this+24]   ; fronts RELÂCHÉS
bl   sub_8234D210
[this+116] = [this+28]   ; courants deviennent précédents
```

## 2. Décalages confirmés

| décalage | contenu |
|---|---|
| `+68` | `packet_number` (4 octets) |
| `+72` | **champ `buttons`** (demi-mot) — cohérent avec la disposition `X_INPUT_STATE` |
| `+20` | fronts appuyés |
| `+24` | fronts relâchés |
| `+28` | boutons courants |
| `+116` | boutons précédents |

## 3. Conséquence, et ce qu'elle exclut

L'invité **n'utilise pas `packet_number` pour ses fronts de boutons** : il
compare directement le demi-mot `buttons` à sa copie précédente. Le calcul est
un ET/OU-exclusif classique, sans masque ni filtre : **tous les bits sont
traités de la même façon**, croix directionnelle et boutons de face compris.

Il n'existe donc ici **aucun endroit où les boutons de face pourraient être
écartés**. Puisque la croix fonctionne, les fronts de A sont calculés eux aussi.

Cela déplace le défaut **en aval** : dans `sub_8234D210`, qui consomme les
fronts, ou chez le lecteur de `[this+20]`.

## 4. Nuance sur le cycle 423

La correction de `packet_number` reste juste au regard du contrat XInput, mais
ce chemin-ci ne s'en sert pas. Son absence d'effet sur la validation est donc
**attendue**, et non un indice supplémentaire.

## 5. Suite

Lire `sub_8234D210` : c'est le premier consommateur des fronts, appelé à chaque
scrutation réussie, et le dernier maillon avant que le jeu n'agisse.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

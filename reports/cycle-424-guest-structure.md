# Cycle 424 — structure de l'objet manette côté invité

## 1. Lecture de `sub_8234D3F0` (arbre `generated/`)

```
r3 = [this+4]                  ; index d'utilisateur
r4 = this+68                   ; tampon X_INPUT_STATE
bl sub_823911C0                ; = XamInputGetState
r3 == 0     -> r30 = 0         ; connectée
r3 == 1167  -> r30 = -1        ; 0x48F = ERROR_DEVICE_NOT_CONNECTED
sinon       -> r30 = -2
puis, si connectée : bl sub_8234D378, et si [this+112] != 0 : bl sub_8234D310
```

## 2. Ce que cela fixe

| décalage | contenu |
|---|---|
| `+4` | index d'utilisateur |
| `+8` | état de connexion (0, −1, −2) — le sélecteur du cycle 413 |
| `+68` | **début du `X_INPUT_STATE`**, donc `packet_number` |
| `+84` | copie de `[this+68]`, écrite par l'aiguilleur |
| `+112` | drapeau conditionnant `sub_8234D310` |

La valeur 1167 confirme indépendamment le modèle d'emplacements du cycle 413 :
`ERROR_DEVICE_NOT_CONNECTED` est bien ce que reçoivent les trois emplacements
vides, d'où leur état −1 permanent.

## 3. Le point important

`[this+84] = [this+68]` mémorise le **`packet_number` précédent**. L'invité fait
donc sa propre détection de front sur ce champ.

C'est exactement le champ dont le cycle 423 a corrigé la sémantique : il
n'avançait qu'à tort, à chaque scrutation. La correction est donc **pertinente
pour ce code**, même si elle n'a pas suffi à débloquer la validation.

Je ne conclus pas qu'elle est sans effet ici : je constate qu'elle ne suffit
pas.

## 4. Suite immédiate

`sub_8234D378` est appelée à chaque scrutation réussie, avec `this` : c'est le
consommateur du `X_INPUT_STATE` fraîchement rempli, et donc l'endroit où les
boutons de face sont traités — ou ignorés.

C'est la fonction à lire ensuite, et elle est courte d'après le voisinage des
adresses (`0x8234D378` à `0x8234D3F0`, soit 120 octets).

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

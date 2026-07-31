# Cycle 435 — l'`overlapped` est **dans** l'objet, à `+92`

## 1. Les adresses se recoupent exactement

Objet relevé au cycle 417 : `obj = 0xA3300060`.
Arguments de l'appel : `device_id_ptr = A33000B8`, `overlapped = A33000BC`.

```
0xA3300060 + 88 = 0xA33000B8    ; device_id_ptr
0xA3300060 + 92 = 0xA33000BC    ; overlapped
```

Et `sub_821CE8A8` fait précisément, avant l'appel :

```
r7 = this+88
[this+88] = 0                   ; device_id remis à zéro
[this+92 .. +116] = 0           ; 7 mots -> la structure X_OVERLAPPED
```

Sept mots de 4 octets = 28 octets, la taille d'un `X_OVERLAPPED`. La
correspondance est exacte sur les trois points : adresse, mise à zéro, taille.

## 2. Disposition consolidée de l'objet sélecteur

| décalage | contenu | établi au cycle |
|---|---|---|
| `+36` | argument passé au sélecteur | 428 |
| `+84` | **état** (0 repos, 1 démarrage, 2 attente) | 428 |
| `+88` | `device_id` reçu | ici |
| `+92` | **`X_OVERLAPPED` embarqué** (7 mots) | ici |

## 3. Ce que cela ferme

L'invité n'a donc besoin d'aucun descripteur externe pour scruter : sa structure
d'attente est un champ de l'objet, à portée immédiate de la routine de mise à
jour. Une scrutation aurait donc la forme « lire `[this+92]`, et si le résultat
n'est plus `IO_PENDING`, remettre `[this+84]` à 0 ».

Aucun code de cette forme n'a été trouvé. Le cycle 432 a montré que la seule
lecture de `[obj+84]` mène à un retour immédiat, et le cycle 434 que ni
événement ni routine de complétion n'existent.

## 4. État du dossier, en une phrase

L'invité a préparé une attente par scrutation entièrement locale — état à `+84`,
`overlapped` à `+92` — la complétion écrit bien `SUCCESS` dans cette structure,
et **rien ne la relit**.

## 5. Reprise

Chercher les lectures de `[X+92]` ou de `[X+96]` (le champ résultat de
l'`overlapped`) dans le voisinage de `sub_821C56F8` et `sub_821CE8A8`. Si aucune
n'existe, la fonction de scrutation manque à l'appel dans notre recompilation —
ce qui serait un défaut de codegen, et non de logique invitée.

Cette dernière possibilité n'a jamais été envisagée dans la série et mérite
d'être écartée explicitement : `rexglue` peut avoir omis une fonction atteinte
uniquement par appel indirect.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

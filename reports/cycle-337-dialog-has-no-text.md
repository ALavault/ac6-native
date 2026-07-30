# Cycle 337 — le dialogue ne réagit à aucun bouton, et il est **vide**

## 0. Fait rapporté par l'opérateur, décisif

**Le dialogue n'a pas de texte.** Deux boutons OUI/NON sur un panneau bleu, et
au-dessus, rien. Sur console, ce dialogue pose une question.

Cela réoriente le diagnostic : ce n'est probablement pas un problème d'entrée.

## 1. Ce que les boutons font, mesuré

Tous atteignent l'invité, vérifié à la frontière `XamInputGetState` :

```
buttons=0x0010  START       2x
buttons=0x1000  A           6x
buttons=0x2000  B           1x
buttons=0x0004  DPAD_LEFT   1x   (cycle 336)
FATAL                       0
```

| action | effet à l'écran |
|---|---:|
| `DPAD_LEFT` | **7,700** — la sélection passe de NON à OUI |
| `A`, taps courts et maintien 2 s | 0,45 – 0,75 |
| `B` | 0,49 |
| `START` | 0,81 |

**La navigation répond ; aucune validation ne répond.** Et ce n'est ni le
bouton ni le front : A, B et START sont reçus, en appui court comme en appui
long, sans effet distinguable du bruit d'animation.

## 2. Le chemin de stockage n'est pas en cause

L'invité appelle, autour du dialogue :

```
XamShowDeviceSelectorUI     1x
XamContentCreateEnumerator  1x
XamDispatchHeadless         2x
XamNotifyCreateListener     3x
```

`XamShowDeviceSelectorUI` est implémenté et rend `X_ERROR_SUCCESS` avec
`device_id = 1` par `xeXamDispatchHeadless`. L'hypothèse « la validation attend
un périphérique de sauvegarde absent » est donc **écartée** : le sélecteur
répond, et il répond avec succès.

## 3. L'hypothèse que le texte manquant rend probable

Un dialogue dont la chaîne n'est pas résolue est un dialogue **incomplètement
construit**. S'il a été bâti sans son texte, il peut tout aussi bien avoir été
bâti sans l'action associée à ses boutons — la sélection étant un état local
purement visuel, elle continuerait de fonctionner, ce qui est exactement le
motif observé : **navigation vivante, validation inerte**.

Autrement dit, le défaut candidat n'est plus dans l'entrée mais dans la
**résolution des ressources de texte de l'interface**.

Aucun journal ne le signale : recherche de `font`, `glyph`, `language`,
`locale`, `xdbf`, `string` dans les 3 000 lignes de l'exécution — **aucune
occurrence**. Les 3 838 correspondances de « text » sont toutes « texture ».
Le texte manquant n'est donc **rapporté nulle part comme un échec**, ce qui est
en soi le résultat de ce cycle : un chemin de ressources échoue en silence.

## 4. Front suivant

1. Identifier comment AC6 construit le texte de ce dialogue — chaîne `.xdbf`,
   police maison, ou texture pré-rendue — en partant des acquis statiques
   existants plutôt qu'en instrumentant à l'aveugle.
2. Instrumenter la résolution de cette ressource, et **échouer bruyamment**
   quand elle est absente : c'est la leçon des cycles 312, 329 et 333, et elle
   n'a pas encore été appliquée au chemin des ressources d'interface.
3. Ne reprendre l'hypothèse « entrée » qu'après : elle est réfutée pour
   A, B et START, en appui court et long, avec receipts à l'appui.

## 5. Ce qui reste

P1.3 non franchie, sélecteur de campagne non atteint. P2 à P7 non faits.
La première mission ne se joue pas.

`recompiler-generated` n'est pas `verified`.

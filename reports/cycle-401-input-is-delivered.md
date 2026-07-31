# Cycle 401 — correction : l'entrée EST délivrée sur ce dialogue

## 1. Ce que le cycle 400 a conclu de trop

Le cycle 400 a établi correctement que **l'écran ne réagit à aucune touche**.
J'en ai déduit que « l'invité ne traite pas les entrées sur cet écran ». La
mesure portait sur des pixels ; la conclusion portait sur le noyau. C'est un
pas de trop, et il est faux.

## 2. Mesure au bon niveau

Sonde `[xam-input]`, à la frontière où le masque de boutons est écrit dans le
`X_INPUT_STATE` de l'invité. Six pressions sur le dialogue :

| ordre pressé | masque journalisé | signification |
|---|---|---|
| space | `0x1000` | A |
| Left | `0x0004` | DPAD_LEFT |
| space | `0x1000` | A |
| Down | `0x0002` | DPAD_DOWN |
| space | `0x1000` | A |
| Escape | `0x0010` | START |

15 lignes avant, 27 après : **12 transitions pour 6 pressions** — une à
l'appui, une au relâchement. `result=0x0` (SUCCESS) partout, `state_ptr=yes`.

La correspondance entre l'ordre pressé et l'ordre journalisé est **exacte, terme
à terme**. L'entrée est délivrée à l'invité, correctement encodée, sans erreur.

## 3. État réel du problème

- l'entrée parvient à l'invité ✔ (mesuré ici)
- l'écran ne change pas ✔ (mesuré au cycle 400, avec témoin de touches non affectées)
- le panneau est vide et déborde à droite ✔ (mesuré au cycle 397)

Donc : **la logique invitée reçoit les boutons et n'en fait rien.** L'écran est
construit à moitié et figé dans un état qui ignore l'entrée — le profil d'une
attente sur une opération asynchrone qui ne se termine jamais.

## 4. Quatrième rétractation, mais prise à temps

| cycle | erreur | détectée par |
|---|---|---|
| 394 | textures accusées à tort | test de suppression |
| 397 | navigateur « non soumis » | retrait de la surimpression |
| 399 | échelle NDC « nulle » | lecture du code |
| 400 | « pas de boucle d'entrée » | sonde au niveau noyau |

Différence utile : celle-ci a été trouvée **au pas suivant**, en testant
l'inférence au lieu de bâtir dessus. Les trois précédentes avaient survécu des
dizaines de cycles. La règle qui marche est simple — mesurer au niveau où porte
l'affirmation, pas un niveau au-dessus.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

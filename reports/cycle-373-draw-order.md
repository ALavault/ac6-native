# Cycle 373 — l'ordre de dessin affaiblit l'occlusion

## 1. Mesure

Ordre de soumission dans la passe défaillante, cycle stable de sept dessins par
trame :

```
1. 028D0000  art    4 sommets
2. 028E9000  art    4
3. 0294A000  art    8
4. 02953000  art    4
5. 03514000  GLYPHES  20
6. 028B2000  art    4
7. 028B7000  GLYPHES  64      <- dernier dessin de la passe
```

## 2. Lecture

`028B7000` — le lot de seize quads, la plus longue chaîne de texte — est le
**dernier dessin de la passe**. Aucun dessin de cette passe ne le suit, et il
reste invisible.

**L'occlusion par un dessin ultérieur de la même passe ne peut donc pas
l'expliquer.** L'hypothèse avancée au cycle 372 est affaiblie ; je la retire
comme explication générale.

Elle reste concevable pour `03514000` seul, suivi par `028B2000` (un quad, art) :
si ce quad recouvrait la zone, il masquerait cette chaîne-là. Mais une cause qui
n'explique qu'un des deux échecs a déjà été écartée deux fois dans cette enquête
(cycles 371, 372) et ne mérite pas d'être retenue seule.

## 3. Ce qui reste

**L'exécution de la copie.** Le *load shader* est choisi, le pipeline existe, la
disposition hôte concorde avec l'invitée — mais rien n'a jamais prouvé que la
dispatch de chargement **s'exécute** et **écrit** l'image pour ces deux
textures. C'est le seul maillon de la chaîne qui n'a pas été observé
directement, et il est le dernier candidat cohérent avec vingt causes écartées.

## 4. Front suivant, précis

Journaliser, dans `LoadTextureDataFromResidentMemoryImpl`, juste après
l'enregistrement de la dispatch : le nombre de groupes de travail émis et la
plage de l'image écrite, pour les sept textures. Si les deux fautives émettent
zéro groupe — ou écrivent une plage vide — la cause est établie.

Contrôle obligatoire, comme depuis le cycle 352 : journaliser aussi les cinq qui
fonctionnent, sinon un zéro ne voudra rien dire.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

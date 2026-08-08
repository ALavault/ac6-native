# Cycle 1138 — la copie mémoire : un chemin de données que les balayages ne voyaient pas

Date : 2026-08-08. Cycle autonome. La prise nommée au cycle 1137, prise.

## Qualification

- Image : Xbox 360 PAL `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Lecture dans `ghidra-projects-xenon/ac6-xenon`, **hors du projet canonique**.
- **Statique seul.** Aucun oracle.

## La primitive

`0x82382F70` est `memcpy` : prologue d'alignement, préchargement `dcbt`, boucles
d'octets puis de mots. Vérifié sur son code.

Un balayage de tous les `bl` dont `r5` vaut 12, 16, 48 ou 64 — les tailles d'un
triplet, d'un vecteur, de trois lignes, d'une transformation complète — rend
553 appels. **Dont un faux positif massif à écarter d'emblée** : les 356 appels
à `0x82222E98` avec `r5 = 16` ne copient rien ; `16` y est l'**alignement** de
l'allocateur, pas une taille. C'est le cinquième défaut d'instrument de cette
série, trouvé avant d'avoir conclu plutôt qu'après.

Restent 38 appels à `memcpy` de taille compatible avec une transformation, et
**un seul est dans le code de la classe d'unité** : `0x822A6090`.

## Ce qu'il fait

```
822a60a4  cmplwi cr6,r4,0x0     ; source nulle : ne rien faire
822a60ac  addi r3,r31,0x4       ; destination = this+0x04
822a60b0  li r5,0x40            ; 64 octets
822a60b4  bl 0x82382f70         ; memcpy
822a60bc  stb r11,0x69(r31)     ; et un drapeau : this+0x69 = 1
```

Soixante-quatre octets — la taille exacte d'une transformation à quatre lignes —
vers `this+0x04`, suivis d'un drapeau « rempli ».

## D'où vient la source

Son unique appelant est **`0x820A8678`**, l'un des trois auxiliaires que la
boucle de construction `0x820A7070` appelle. À `0x820A8928` :

```
820a88dc  li r4,0x3 ; bl 0x82234dd0   ; l'enfant 3 d'une ressource
820a88f4  bl 0x82234e08               ; sa taille
820a8920  addi r4,r31,0x2b0           ; source = enfant3 + 0x2B0
820a8924  addi r3,r11,0x1ac           ; destination = [r23+0x1330] + 0x1AC
820a8928  bl 0x822a6090               ; -> memcpy(dest+0x04, source, 0x40)
```

**C'est un chemin de données du chargement que dix cycles de balayage de
magasins ne pouvaient pas voir** : soixante-quatre octets d'un fichier
d'archive vers une structure, pendant la construction des unités.

## Ce que ce n'est pas

La ressource n'est **pas** le paquet FHM de la mission. Son enfant 3 y ferait
32 octets, et cette lecture prend un déplacement `+0x2B0`. C'est donc une autre
ressource — vraisemblablement celle de l'appareil, puisque l'appel est dans la
construction par unité — et **rien n'établit que ces 64 octets soient une
position**. La destination `[r23+0x1330]+0x1AC` n'est pas non plus la
transformation d'un objet, qui vit en `+0x20..+0x50`.

## Ce que le cycle établit

1. `memcpy` est `0x82382F70`, et la question « une position est-elle copiée en
   bloc ? » est désormais **posable**.
2. Trente-huit appels ont une taille compatible ; un seul touche le code de la
   classe d'unité, et il écrit ailleurs que la transformation.
3. Donc **aucun `memcpy` de taille de transformation n'écrit la transformation
   d'une unité** dans ce binaire.

Avec les cycles 1136 et 1137, l'énumération couvre maintenant les quatre idiomes
— `stvx128`, `stfs` littéral, `stfsx` indexé, `memcpy` — et **aucun n'écrit la
position d'une unité depuis des données de mission**.

## Ce que cela n'établit pas

- Que la liste des idiomes soit close. Un `memcpy` de taille **variable**, ou
  une copie écrite à la main, échappe encore.
- Ce que sont les 64 octets copiés depuis l'enfant 3.

## Décision de cycle

Rien n'est porté. Le résultat est une fermeture, et elle est écrite dans
`WORLD_POSITION_DEBT.md` plutôt que dans dix rapports.

`ctest 24/24`, la porte JF reste verte.

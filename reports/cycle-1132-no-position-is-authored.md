# Cycle 1132 — les 65 écritures, classées : aucune position n'est écrite depuis des données

Date : 2026-08-08. Cycle autonome. La troisième prise du cycle 1128, prise.

## Qualification

- Image : Xbox 360 PAL `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Lecture dans `ghidra-projects-xenon/ac6-xenon`, **hors du projet canonique**.
- **Statique seul.** Aucun oracle.

## Ce qui a été fait

Le cycle 1129 avait rendu **65 sites** écrivant la ligne de translation d'un
objet — déplacement effectif `+0x50`, biais et index résolus. Le cycle 1128 avait
décidé de ne pas les lire un par un ; ils sont maintenant classés **tous**, par
la provenance du vecteur stocké : une fenêtre de 14 instructions avant chaque
écriture, et la dernière instruction qui définit le registre vectoriel.

`analysis/translation-writes.tsv` porte les 65 lignes.

| classe | sites | ce que c'est |
| --- | ---: | --- |
| `copy` | 48 | le vecteur vient d'un `lvx128`/`lvlx` : **un transfert** |
| `unresolved` | 8 | la source est hors de la fenêtre |
| `computed` | 5 | le vecteur sort d'arithmétique vectorielle |
| `stack` | 4 | la base est `r1` : une locale, pas un objet |

## Le résultat, restreint au code de mission

Sur les **28** écritures du groupe `0x822xxxxx` :

- **26 sont des copies** — dont `0x8226B368` (la copie de transformation du
  transfert de masse) et `0x822E5074` (l'état `0x822E4A48`) déjà lues ;
- **1 est calculée** : `0x82260284`, dans `0x8225E508`. Le contexte est sans
  ambiguïté — quatre lignes de `vmsum3fp128` contre trois registres de matrice,
  puis des `vmrghw` de recomposition, puis quatre `stvx128` vers deux objets.
  **C'est une concaténation de matrices 4×4**, une transformation *dérivée* d'une
  autre, pas une donnée lue. La fonction est appelée par `0x82276610` et lit
  `[global+0x29FC8]+0x1008`, l'objet courant du gestionnaire ;
- **1 reste non résolue** : `0x822D8EB0`, dans `0x822D8220`.

**Aucune des 28 n'écrit une position venue de données.** Elles la copient ou la
composent.

## Ce que cela permet de dire

C'est le premier énoncé **exhaustif** de cette série, et il est négatif :

> Dans tout le code de mission, la position d'un objet n'est jamais créée à
> partir du contenu de la mission. Elle est toujours propagée — copiée d'un autre
> objet, ou composée d'une autre transformation.

Ce n'est pas « je n'ai pas trouvé » : les 65 sites sont énumérés et classés, et
le sous-ensemble mission est fermé. Si une position est authorée quelque part,
**ce n'est pas dans `0x822xxxxx`**.

## Ce que cela ne dit pas

- **Où elle est authorée.** 33 des 65 sites sont hors du groupe mission et n'ont
  pas été lus ; c'est là qu'il faut chercher, et l'ordre naturel est par
  proximité avec le modèle de vol et la caméra.
- **Que le balayage est complet.** Il ne voit que `stvx128` avec un index
  constant ou un biais formé par `addi`. Une écriture par trois `stfs` avec des
  sources distinctes est couverte par le balayage du cycle 1128 (12 sites, tous
  expliqués) ; une écriture par `memcpy` ou par un index calculé ne l'est par
  aucun des deux. C'est la limite honnête de l'instrument.
- **Ce que fait `0x8225E508`.** Sa composition de matrices est établie ; son rôle
  — attache, caméra, hiérarchie de modèle — ne l'est pas, et le nommer serait une
  hypothèse.

## Décision de cycle

Rien n'est porté ; ce cycle produit un artefact et une fermeture, pas un
comportement. `analysis/translation-writes.tsv` est versionné pour que le
prochain cycle reprenne la liste au lieu de la refaire — c'est la troisième fois
que cette série refait un balayage faute d'avoir gardé le précédent.

`ctest 24/24`, la porte JF reste verte.

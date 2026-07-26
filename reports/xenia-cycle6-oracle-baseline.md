# AC6 PAL — baseline Xenia cycle 6

## Contrat exécuté

`scripts/run_xenia_ac6_oracle_baseline.sh` a lancé le `default.xex` PAL dans un
Xvfb isolé, avec captures à 4 s et 9 s. La sortie locale non versionnée est
`.build/xenia-ac6-cycle6-absolute/`; son `manifest.json` lie le XEX
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` à Xenia
Canary `98559834c570d4be8ba5d532f000aadf8ea6cf4d495be34a02b7ae766134007c`.

Les deux PNG 1280x720 ont le même SHA-256 :
`42fa11d06dad4ca1a1793a84882aa8a1fcb6eaeeb7373dbec443d2150e448a6b`.
L'inspection visuelle montre uniquement la fenêtre/menu Xenia sur fond noir;
aucune scène retail, menu du jeu, frame Xenos ou entrée ne peut être attribué.

## Correction de reproductibilité

Le runner normalise désormais une sortie relative contre la racine du dépôt
avant de passer `HOME`/XDG à Xenia. Le rejet antérieur de `HOME` relatif ne se
reproduit pas dans le log du baseline final.

## Statut et suite

Statut : `blocked-oracle` pour le boot de jeu visible, mais oracle de lancement
`dynamically-confirmed` au sens étroit (module lancé et captures attribuables).
Ce résultat ne modifie pas le statut statique `manual-review` de la frontière
Ghidra `82382ef8` et ne constitue pas une preuve de parité.

Prochaine action : résoudre la frontière/noreturn dans le projet XEX, puis
ajouter un replay d'entrée borné et une condition de frame non noire avant une
nouvelle capture plus longue.

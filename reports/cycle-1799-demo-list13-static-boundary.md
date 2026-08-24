# Cycle 1799 — frontière statique de la liste 13

Le sous-enfant naturel `0x2E3F1350` sélectionne au tick 3001 un élément type
4 dont `list_index=13`. Le chemin commun est qualifié jusqu'à la table
matérialisée `0x2DD7963C` et au dispatch interne `0x8264D074`, grâce au cas
homologue index 15. Les deux mots de `table[13]` à `0x2DD796A4/+0x04` et le
record désigné sont absents des artefacts statiques conservés.

`TitleUS` est bien une ressource de la démo PAL : `DATA.TBL[177]` contient
`Title`, `DATA.TBL[178]` contient `TitleUS`, sélectionné par la configuration
anglaise. Le suffixe ne qualifie pas la région du XEX.

La prochaine expérience est un snapshot read-only unique au tick 3001 des
deux mots du descripteur, du compte de liste et de huit records maximum. Le
gate statique est fermé sur cette frontière ; aucune valeur n'est inférée du
cas index 15.

État : `frontend=false`, `mission=false`, `terminal=false`,
`supported=false`.


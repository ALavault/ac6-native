
# Cycle 1112 — les parentés manquantes, résolues

Date : 2026-08-09. Le cycle 1111 rendait 10 arêtes sur 33 et disait les autres
« non résolues, pas absentes ». Elles le sont maintenant : **36 arêtes, un seul
arbre, aucun cycle.**

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- **Statique seul.** Artefact : `analysis/state-machine/mission-state-map.tsv`.

## Pourquoi 23 manquaient

Rien de subtil : **le répartiteur de signal a trois formes**, et je n'en
reconnaissais qu'une.

**1. Chaîne de `beq`** — le chemin par défaut suit en séquence.

```
822eacd4  cmpwi cr6,r5,-0x5
822eacdc  beq   cr6,0x822eb008
822eace8  cmpwi cr6,r5,-0x1
822eacec  beq   cr6,0x822ead34
822eacf0  lis   r10,-0x7dd1     ← le défaut, juste après
```

**2. `bne` final** — le gestionnaire de sortie est en séquence, et **le défaut
est à la cible du branchement**. C'est ce qui manquait pour la majorité :

```
822e7788  cmpwi cr6,r5,-0x1
822e778c  bne   cr6,0x822e78e0   ← le défaut est là-bas
822e7790  addi  r11,r31,0x7f0    ← ici, c'est la sortie
...
822e78e0  lis   r10,-0x7dd1      ← le défaut
822e78e8  subi  r10,r10,0x28f8   → 0x822ED708
```

**3. Table de saut** — le signal est biaisé de 5 pour ramener `-5..-1` sur
`0..4`, borné, puis indexé ; le défaut est à la cible du `bgt` :

```
822ea978+  addi   r11,r5,0x5
           cmplwi cr6,r11,0x4
           bgt    cr6,0x822eac74   ← le défaut
           lis    r12,-0x7dd1      ; la table
           lwzx   r0,r12,r0 ; bctr
```

Les cinq « racines » secondaires du cycle 1111 étaient exactement les
gestionnaires à table de saut.

## Le contrôle

Le même qu'au cycle 1111, et c'est lui qui donne sa valeur au résultat : une
relation de parenté **doit** être acyclique. La règle rejetée au cycle 1111 en
produisait dix. Celle-ci, sur 36 arêtes :

```
états 39 | arêtes 36 | non résolus 3 | cycles 0 | racines 1 | profondeur max 6
```

## L'arbre

```
0x822E39A8                     racine, non enregistrée par .pdata
├── 0x822E4A48   code 4
├── 0x822E5280   code 10
├── 0x822E7598   code 9
├── 0x822E8660   code 3
│   └── 0x822E73B0   code 11
├── 0x822ED708   code 1
│   ├── 0x822E70B0   code 12
│   ├── 0x822E7760   code 8       ← l'état où la mission tourne
│   ├── 0x822EACC0   code 7
│   │   ├── 0x822E6E98
│   │   └── 0x822EA648 → 0x822E80A0, 0x822E8F80, 0x822E9A30
│   ├── 0x822EB090   code 6
│   │   ├── 0x822E6AB8
│   │   └── 0x822EA978 → 0x822E8CD8, 0x822E9720, 0x822EA220
│   ├── 0x822EB5A8   code 2
│   │   ├── 0x822E9DF8 → 0x822E6870, 0x822E9458 → 0x822E7AF8 → 0x822E7E68, 0x822E8A90
│   │   └── 0x822EBD10
│   └── 0x822ED070   code 5
│       ├── 0x822E6CC0
│       └── 0x822ECD90 → 0x822EC090, 0x822EC4D0, 0x822EC998
├── 0x822EDC58
└── 0x822EE5C0
```

`0x822ED708` est le super-état intermédiaire qui porte presque tout : six
enfants directs, et par eux la quasi-totalité des trente-neuf états.

## Une attribution qui n'est plus une supposition

Le cycle 1111 signalait que les codes n'étaient pas attribués par branche. Pour
le seul qui comptait, c'est fait — la branche `-3` de `0x822E7760` est lisible :

```
822e778c  bne cr6,0x822e78e0    ; ce n'est pas la sortie
822e78f4  li  r11,0x8           ; branche d'entrée
822e78fc  stw r11,0x260(r31)
```

**`0x822E7760` écrit bien 8 en entrée**, ce qui confirme par lecture directe
l'identification que le cycle 1111 tirait d'un recoupement avec le cycle 1093.

## Ce que cela n'établit pas

- **Trois états restent sans parent** : `0x822E3560` (code 1), `0x822E35F8` et
  `0x822E3640` (code 14 tous deux). Ils sont groupés à part dans l'espace
  d'adresses et n'entrent pas dans l'arbre ; rien ne dit s'ils appartiennent à
  une autre machine ou si leur défaut prend une quatrième forme.
- **Les codes des autres états restent non attribués par branche.** Seuls
  `0x822E8660 → 3`, `0x822EACC0 → 7` et `0x822E7760 → 8` sont vérifiés par
  lecture ; la valeur `1` revient chez huit états et n'est probablement pas leur
  code d'entrée.
- **Aucun état n'est nommé.** L'arbre a une forme, pas un vocabulaire.
- Les **transitions** — quel état saute vers quel autre — restent mêlées aux
  arêtes de parenté dans le graphe de références ; seul le lien parent est
  séparé ici.


# Cycle 1113 — les trois parentés manquantes n'existent pas

Date : 2026-08-09. La question était « trouve les 3 parentés restantes ». La
réponse est qu'il n'y en a pas à trouver : **ces trois fonctions ne sont pas des
états.**

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- **Statique seul.**

## La signature les disqualifie

Un gestionnaire d'état de cette machine prend trois arguments — le tampon de
sortie, l'objet, le signal — et commute sur le troisième. `0x822E3560`,
`0x822E35F8` et `0x822E3640` n'en prennent **qu'un** :

```
822e3560  mfspr r12,LR
822e356c  stwu  r1,-0x60(r1)
822e3570  or    r31,r3,r3      ; l'objet, et rien d'autre
822e3574  bl    0x8226b618     ; r5 n'est jamais lu
822e3578  li    r11,0x1
822e358c  stw   r11,0x260(r31) ; écrit le code d'état
```

Le graphe de références le confirme sans ambiguïté :

```
AC6_NODE 822e3560 engine_calls=0 signal_tests=0 data=[] call=[]
AC6_NODE 822e35f8 engine_calls=0 signal_tests=0 data=[] call=[]
AC6_NODE 822e3640 engine_calls=0 signal_tests=0 data=[] call=[]
```

Aucun test de signal, aucun appel au moteur de transition, aucun état nommé.
Elles n'ont pas de super-état parce qu'elles n'ont pas de place dans la
hiérarchie.

## Ce qu'elles sont

Aucune n'a d'appelant direct. Chacune est pointée **deux fois** dans `.rdata` :

| fonction | pointeurs |
| --- | --- |
| `0x822E3560` | `0x82064420`, `0x820644E8` |
| `0x822E3640` | `0x82064424`, `0x820644EC` |
| `0x822E35F8` | `0x820644A8`, `0x82064570` |

Les deux séries sont séparées de `0xC8` exactement — **deux tables virtuelles de
même disposition**, chacune portant les trois aux mêmes emplacements relatifs.
Ce sont donc des **méthodes virtuelles**, appelées par slot.

## Ce que cela change

`contexte+0x260` n'est **pas** maintenu par la seule machine à états. Deux
chemins l'écrivent :

- les **branches d'entrée** des 36 états, via le moteur de transition ;
- ces **trois méthodes virtuelles**, directement, sans transition.

C'est une nuance qui compte pour quiconque lira ce champ comme « l'état
courant » : il publie un code, et ce code a deux sources.

## Correction du dénombrement

Les cycles 1111 et 1112 parlaient de « 39 états ». Le nombre venait de mon
extracteur, qui retenait toute fonction écrivant `+0x260` — un critère de *code
d'état*, pas de *gestionnaire d'état*. Le compte juste est :

| | |
| --- | ---: |
| gestionnaires d'état | **36** |
| arêtes de parenté | **36** |
| non résolus | **0** |
| méthodes qui posent un code sans être un état | 3 |

L'arbre du cycle 1112 est donc **complet** : tout état a un parent, et la racine
`0x822E39A8` est le seul sommet.

L'artefact `analysis/state-machine/mission-state-map.tsv` porte désormais une
colonne `kind` qui distingue `state` de `setter`.

## Ce que cela n'établit pas

- **À quelle classe appartiennent les deux vtables.** Aucun localisateur RTTI
  ne précède la zone examinée, et la chaîne `UpTaskGame` trouvée en
  `0x8206421C` est adjacente sans qu'aucune preuve ne la rattache à ces tables.
  Je la signale parce qu'elle est là, pas parce qu'elle les nomme.
- Quand ces méthodes sont appelées, ni par qui : elles n'ont que des appels
  virtuels.
- Pourquoi le code 14 est publié par deux fonctions distinctes
  (`0x822E35F8` et `0x822E3640`).

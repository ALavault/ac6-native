
# Cycle 1117 — le gate JF, avec une dérivation vérifiable

Date : 2026-08-09. Premier cycle sous l'objectif JF : construire la porte qu'il
nomme, et la rendre difficile à satisfaire mécaniquement.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- **Statique et produit natif seuls.** Aucun oracle.

## Le problème de la pièce `derivation`

Le goal demande que chaque comportement « cite la fonction retail dont il
dérive ». Une citation **dans le contrat** ne prouve rien : le contrat est
précisément ce qu'on audite. Un domaine pourrait déclarer n'importe quoi.

D'où la règle retenue : **la source native elle-même doit porter l'adresse**.
L'auditeur ouvre le fichier cité et exige d'y trouver chaque adresse retail que
le comportement revendique ; il refuse par ailleurs tout chemin marqué comme
généré ou recompilé (`ac6-recomp-reference`, `/generated/`, `ppc_recomp`,
`XenonRecomp`), ce qui applique l'anti-but 1 mécaniquement plutôt qu'à la
confiance.

## Les trois contrôles négatifs

Une porte qu'on n'a pas vue refuser ne vaut rien. Les trois cas :

```
adresse absente du fichier   → fail: ... never cites retail address 0xDEADBEE0
dérivation vers du généré    → fail: ... never cites retail address 0x82267468
passed sans dérivation       → fail: mission_counters passed without ['derivation']
```

Le deuxième mérite un mot : le chemin généré échoue **aussi** sur l'absence
d'adresse, donc le refus est doublement motivé.

## L'état réel

```
mission01_final_gate=audit-valid JF=open
open=mission_state_machine,mission_area,playable_session,mission_completion
```

**Quatre comportements dérivés**, chacun avec sa preuve statique, son artefact
de test natif et une source qui cite ses adresses :

| comportement | adresses retail | source native |
| --- | --- | --- |
| `scenario_container` | `0x82249718`, `0x82330158`, `0x8232EC08`, `0x82330A30` | `src/retail_bin_readers.cpp` |
| `unit_construction` | `0x820A7070`, `0x820A7F48`, `0x8226FEC0`, `0x820A7420` | `src/retail_mission_state.cpp` |
| `sub_mission_flow` | `0x8226E908`, `0x8226E158`, `0x82267008` | `src/retail_mission_state.cpp` |
| `mission_counters` | `0x82267468`, `0x82380798`, `0x82008120` | `src/retail_mission_state.cpp` |

Les citations ont été **déplacées des en-têtes vers les implémentations** :
elles vivaient dans les `.h`, alors que le comportement vit dans les `.cpp`.
C'est ce que JF demande — la provenance là où est le code.

**Quatre comportements ouverts**, avec leur raison inscrite au contrat :

- `mission_state_machine` — le moteur `0x8219AD20` est lu (cycle 1109) et
  l'arbre d'états cartographié (cycle 1112) ; **rien n'en est porté** ;
- `mission_area` — le rectangle, ses axes et son application sont lus
  (cycles 1105, 1107) et non portés ;
- `playable_session` — le constructeur de monde existe et est testé, mais la
  commande de session d'`ac6-native` consomme encore des manifestes ;
- `mission_completion` — aucun chemin ne mène le séquenceur au bout du script
  ni à un débriefing.

## L'artefact de test natif

Le test jouable émet désormais `reports/mission01-retail/retail-playable.json` :
230 unités depuis le conteneur seul, recensement 140/42/48, 4 objectifs dont
zéro complété, 1 800 pas, empreinte sémantique `4efb07a0a1d370a6`.

## Ce que cela n'établit pas

- **JF n'est pas atteint**, et le rapport le dit avant que quiconque lise le
  vert d'un test voisin : la moitié des comportements du chemin de mission n'a
  pas de dérivation.
- La pièce `derivation` prouve qu'une source **cite** une adresse, pas qu'elle
  la **reproduit**. C'est un garde-fou de provenance, pas une preuve
  d'équivalence ; les preuves d'équivalence restent les contrôles propres à
  chaque cycle — digests, distributions, invariants.
- Les positions monde et les 1 619 vtables rejetées restent les deux dettes
  nommées par le goal, intactes.

# Cycle 514 — le premier record enfant retourne, le suivant n'est pas un index

Date : 2026-08-02. Cible : Xbox 360 PAL `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
Projet Ghidra canonique : `ghidra-projects/ace-combat-6`, programme
`default.xex`, module PAL, fonctions `0x8237C4D8` et `0x8237CC58`.

## Résultat

Le fallback borné du premier enfant atteint maintenant le retour natif de
`sub_8237C4D8`. Le record initial est stable :

- `child=0xB8720180`, `child+8=0x00010004` ;
- propriétaire `0xB8EC8290`, table indirecte `0xB8EC7DDC` ;
- champ de table `+0x38` absent ;
- entrée sparse matérialisée à `0xBF080020`, offset de record nul.

Le deuxième passage de la boucle à `lr=0x8237CEF0` reçoit ensuite
`child=0xB8ED0D10`. Ce n'est plus le record initial : son premier mot est la
vtable `0x8201887C`, `child+4` repointe sur `0xB8720180`, et `child+8` vaut le
pointeur invité `0xB8EC9438`. Le traiter comme l'index `3102512184` produit le
slot invalide `0xC764A1C0`.

Le dispatcher suivant confirme la mauvaise interprétation : avec
`input=0xB8EC9438`, le type lu devient `0x3D9` et la prétendue cible
`0x82019E6C` contient le texte ASCII `No locks available`, pas du code.

## Cross-match ABI

Après qualification du XEX, le C++ généré épinglé est utilisé uniquement comme
cross-match littéral :

- `sub_8237C4D8` sauvegarde l'entrée `r5` dans `r29`, puis lit `[r29+8]` ;
- la boucle PAL à `0x8237CED4` recharge `r3`, mais pas `r5` ni `r6`, avant le
  `bctrl` à `0x8237CEF0` ;
- `sub_8237C4D8` laisse ensuite dans `r5` le nœud obtenu/créé par son chemin
  natif.

Le second passage dépend donc d'un contrat porté par les registres et par le
nœud retourné. Il n'autorise ni une table globale synthétique, ni une écriture
forcée de `child+8`, ni l'assimilation du pointeur à un index.

## Observation visuelle loadout

La capture `step-39-loadout-confirm-1.png` rend le cadre et les axes de
l'hexagone de capacités, mais aucun polygone de valeurs. Les statistiques sont
vides, le panneau droit affiche `---` et les quantités d'armes restent nulles.
Le tracé de base fonctionne donc ; le symptôme est compatible avec des données
avion/loadout non publiées, sans prouver encore qu'il partage la cause du
contrat enfant.

## Validation

- build `ac6recomp` après séparation du log timeline haute fréquence : PASS ;
- run borné cycle 514 : bootstrap, dialogues, mission submenu, avion et arme
  atteints ; aucun `REX_FATAL` ni signal hôte observé avant la nouvelle
  frontière ;
- le marqueur du premier fallback puis celui du second enfant apparaissent
  dans cet ordre dans
  `/fastdata/lavaulta/auto-re-agent/reports/logs/cycle-514-child-record-return/` ;
- la garde de journalisation empêche le flux timeline par frame d'effacer le
  marqueur campagne lorsque seul `ac6_log_ui_dispatch` est actif.

## Prochain checkpoint

Au caller PAL `0x8237CEF0`, enregistrer sur les deux premières itérations le
compte de répétition, `r4/r5/r6`, `[state+216..220]` et le producteur immédiat
de `r6`. Déterminer pourquoi `r6=0xB8EC9438` est recopié dans le nœud retourné.
Corriger uniquement ce producteur ou le contrat de répétition qualifié. Ne pas
réécrire `child+8`, ne pas étendre la table sparse et ne pas relancer un probe
de cible `0x82019E6C`.


# Cycle 1129 — le transfert de masse : un état de mission place tous les objets

Date : 2026-08-08. Cycle autonome. La prise que le cycle 1128 nommait, prise.

## Qualification

- Image : Xbox 360 PAL `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- Lecture dans `ghidra-projects-xenon/ac6-xenon`, **hors du projet canonique**.
- **Statique seul.** Aucun oracle.

## L'instrument qui manquait

Les quatre balayages précédents cherchaient le déplacement `0x50` écrit
littéralement. Or ce binaire écrit ses transformations par **stockage vectoriel
indexé sur un pointeur biaisé** :

```
addi r30,r3,0x10     ; le biais d'accesseur du cycle 1122
li   r7,0x40
stvx128 vr0,r30,r7   ; atteint objet+0x50 sans que 0x50 apparaisse
```

`tools/ghidra_scripts/Ac6TransformWrite.java` suit désormais la formation des
registres — `addi` pour le biais de base, `li` pour l'index, invalidation à
toute autre écriture — et rend l'**offset effectif**. Sur `0x50` : **65 sites**,
dont la quasi-totalité en `base+0x10 / index 0x40`, exactement la forme que les
quatre balayages précédents ne pouvaient pas voir.

C'est le troisième correctif d'instrument de cette série, et le seul qui ait
trouvé quelque chose.

## La chaîne, du haut vers le bas

**`0x822E7760`** — un **état** de l'arbre des 36, profondeur 2 sous
`0x822ED708`, celui dont le cycle 1112 a établi qu'il publie le code d'entrée
**8**, et que le contrat JF cite déjà pour `mission_state_machine`. Il appelle,
à `0x822E7900` :

**`0x8226E950(contexte)`** — la boucle de transfert :

```
8226e968  lwz r11,0x2a0(r27)   ; le gestionnaire source
8226e970  lwz r10,0x4(r11)     ; son compte
8226e980  lwz r10,0x2a4(r27)   ; le gestionnaire destination
8226e984  lwzx r4,r31,r11      ; source[i]
8226e988  lwzx r30,r31,r10     ; destination[i]
8226e990  lwz r28,0x15c(r30)   ; on retient la ressource de la destination
8226e994  bl 0x8226cf90        ; on la recouvre entièrement
8226e998  stw r28,0x15c(r30)   ; on lui rend sa ressource
```

**`0x8226CF90(dst, src)`** — le clonage : la transformation d'abord, puis
`+0xD0` à `+0x140` et au-delà, champ par champ.

**`0x8226B368(dst, src)`** — la transformation, les deux copies :

```
8226b370  addi r31,r4,0x10 ; addi r30,r3,0x10     ; les pointeurs biaisés
8226b398..8226b3b0  les lignes +0x20, +0x30, +0x40
8226b3b4  lvx128 vr0,r31,r7 ; 8226b3b8 stvx128 vr0,r30,r7   ; +0x50, la translation
8226b3bc..8226b3d8  les mêmes quatre lignes de la copie de trame, +0x70..+0xA0
```

**Un état de mission recopie donc l'état complet de tous les objets d'un
gestionnaire sur ceux d'un autre, translations comprises.** C'est le mécanisme
par lequel les objets acquièrent leurs positions en bloc.

## Quels gestionnaires — et pourquoi cela ne clôt pas la question

Le chargeur les publie :

```
8219c78c  stw r27,0x2a0(r28)   ; contexte+0x11D470 = CX360ObjManager n°1
8219c790  stw r24,0x2a4(r28)   ; contexte+0x120858 = CX360ObjManager n°2
8219c780  stw r22,0x2a8(r28)   ; contexte+0x12B440 = CX360UnitManager n°1
8219c788  stw r25,0x2ac(r28)   ; contexte+0x12B85C = CX360UnitManager n°2
```

et les trois appels du cycle 1096 remplissent le n°1 depuis le **slot 0** avec le
sélecteur 0, le n°2 depuis **le même slot 0** avec le sélecteur 1.

**Deux ensembles parallèles bâtis des mêmes 434 enregistrements.** Copier le n°1
sur le n°2 est donc une **remise à l'état** — un ensemble intact et un ensemble
vivant — et non une origine : le cycle 1124 a montré que les deux sortent du
chargeur à l'origine avec la transformation identité.

La question se déplace donc, et elle est enfin étroite : **qu'est-ce qui remplit
les objets du gestionnaire `contexte+0x2A0` ?** Le sélecteur 0 de `0x820A7070`
est le seul des trois à exécuter deux boucles supplémentaires sur le
gestionnaire, par les emplacements virtuels `+0x18` et `+0x1C` ; c'est là que le
prochain cycle regarde.

## Décisions de cycle

Tranchées ici plutôt que posées en question :

1. **Ne pas porter ce mécanisme.** Le produit natif n'a ni double ensemble ni
   état 8 ; le porter reproduirait une copie entre deux mondes qu'il n'a pas.
2. **Ne pas renommer l'état `0x822E7760`** dans `retail_state_machine.cpp`. Ce
   qu'il fait est établi pour un appel ; sa raison d'être — reprise, replay,
   redémarrage — ne l'est pas, et un nom serait une hypothèse.

`ctest 24/24`, la porte JF reste verte.

## Ce que cela n'établit pas

- **L'origine des positions**, toujours pas. Le mécanisme de placement en masse
  est trouvé ; sa source ne l'est pas.
- **Pourquoi deux ensembles.** « Intact » et « vivant » est la lecture évidente
  et elle n'est pas testée ; rien ici ne dit lequel est lequel au-delà du sens du
  transfert.
- **Ce que fait le sélecteur 0** dans ses deux boucles supplémentaires. C'est la
  prise suivante, nommée et non prise.

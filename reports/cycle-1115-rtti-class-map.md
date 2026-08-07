
# Cycle 1115 — la carte de classes, et le verrou VMX128

Date : 2026-08-09. Premier cycle sous l'objectif J2 : ses deux pistes, l'une
avancée, l'autre débloquée.

## Qualification

- Projet Ghidra canonique `ghidra-projects/ace-combat-6`, Xbox 360 PAL
  `default.xex`,
  SHA-256 `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.
- **Statique seul.** Aucun oracle, conformément à la politique reconduite.

## Piste carte : 811 vtables nommées

`scripts/SweepMsvcRtti.java` balaie tout bloc en lecture seule à la recherche du
motif complet — un mot pointant un localisateur, dont le descripteur de type
porte un nom commençant par `.?A`, et dont le premier emplacement de table vise
un bloc exécutable.

```
AC6_RTTI_SUMMARY vtables=811 locator_like_rejected=1619
```

**Les 1 619 rejets sont comptés, pas nommés.** C'est l'anti-but 2 de J2 pris au
mot : un candidat qui échoue à une étape n'entre pas dans la carte.

`analysis/class-map.tsv` porte 811 lignes, **721 classes distinctes**, avec pour
chacune ses bases et leur déplacement. Le démanglage est laissé **vide pour les
48 gabarits** : la colonne mangled fait foi, plutôt qu'un joli nom faux.

La carte reproduit d'elle-même un résultat obtenu autrement au cycle 1114 :

```
0x82064264  ACE6::CAce6MissionManagerCampaign
            base CHsm<CAce6MissionManagerCampaign,8> @ 840   (= 0x348)
```

Et elle complète la famille : à côté de **Campaign** (`CHsm`) et **Replay**, il
existe **`CAce6MissionManagerOnline`**, dont la base est un `CFsm` — une machine
**plate**. Les trois variantes correspondent aux trois constructeurs que
`0x82199F68` choisit selon le mode (cycle 1096).

## Piste jouable : le verrou est VMX128, et il saute

Le goal nommait les positions monde comme seul verrou dur, avec une première
tranche : lire le sous-lecteur d'étiquette 2 en statique.

La chaîne se lit : le répartiteur d'ordres `0x822969F8` envoie l'étiquette 2 à
**`0x82295A88`**, qui prépare un vecteur de trois flottants sur la pile et le
confie à **`0x822953F0`** avec la charge utile de l'ordre. C'est là que se
décide la question — et la décompilation s'arrête net :

```
/* WARNING: Bad instruction - Truncating control flow here */
halt_baddata();
```

Le désassemblage montre pourquoi :

```
822954b4  lfs f0,0x40(r30)      ; un flottant de la charge utile
822954b8  <not-disassembled>
822954bc  <not-disassembled>
822954c0  <not-disassembled>
```

**Ce ne sont pas des trous de corps** (cycle 1110) mais des instructions que le
langage du projet ne décode pas : le calcul vectoriel est en **VMX128**, et le
programme est chargé en `PowerPC:BE:64:A2ALT-32addr`.

L'extension Xenon était présente dans `.tools` **en source et en zip, non
installée**. Installée, elle déclare un langage **séparé**,
`PowerPC:BE:64:Xenon` — elle ne modifie donc pas le projet canonique, ce qui
écarte le risque de perdre vingt cycles d'état qualifié.

Un second projet, `ghidra-projects-xenon/ac6-xenon`, importe le même fichier
avec ce langage. Même image (`.text 0x82090000-0x823D772B`), même exécutable.
Et le bloc muet parle :

```
822954b8  lvlx      v11,r0,r10
822954c0  lvx128    vr0,r0,r11
822954c4  vrlimi128 vr10,vr11,0x4,0x3
822954f4  vpermwi128 vr13,vr11,0x2b
```

## Ce que cela n'établit pas

- **Rien encore sur les positions monde.** Le verrou est levé, la lecture reste
  à faire : il faut monter le corpus Xenon (les 8 246 débuts de fonction depuis
  `.pdata`, passe en cours) avant de décompiler `0x822953F0`.
- La carte ne dit **que** ce que la RTTI porte : des noms, des bases, des
  déplacements. Aucun champ, aucune méthode, aucun rôle.
- Les 1 619 candidats rejetés ne sont pas expliqués un par un ; certains sont
  probablement des tables sans localisateur, d'autres du faux positif de
  balayage.
- Le second projet n'est **pas** le corpus qualifié. Tout fait qui en sortira
  devra citer sa provenance, et rien n'y sera fusionné avec le projet canonique
  — la discipline qui vaut déjà pour `-corrected`.

# Cycle 340 — une frontière déclarée coupe la routine du dialogue en deux

## 1. Fait

`sub_821CE8A8` est la vraie fonction : prologue complet
(`mflr`, `std r30/r31`, `stwu r1,-112(r1)`), 107 lignes.

`rex_sub_821CE8E0` — déclarée dans `[functions]` — **duplique sa queue**. Les
deux corps contiennent le même code à partir de `stw r30,88(r31)` :

```c
// sub_821CE8A8, à loc_821CE8D4 :        // rex_sub_821CE8E0, en entrée :
stw r30,88(r31)                          stw r30,88(r31)
bne cr6,0x821ce8f0                       bne cr6,0x821ce8f0
...                                      ...
```

`rex_sub_821CE8E0` n'a **aucun appelant** dans le corpus et **aucun prologue** :
c'est un bloc de continuation promu en fonction par la configuration, pas un
point d'entrée. Exactement le motif du cycle 312 — « suivre un épilogue, ou
figurer dans une table, est nécessaire mais pas suffisant pour conclure à une
entrée de fonction ».

## 2. Pourquoi cela pourrait compter ici

C'est **une hypothèse, non mesurée** — à traiter comme telle, conformément à la
règle qui a coûté les cycles 309, 312, 317 et 331.

La routine coupée est celle du dialogue : `sub_821CE8A8` initialise l'objet
(`r31+84`, `r31+88`, tampon de 7 mots en `r31+92`) puis appelle
`sub_821F4658` -> `XamShowDeviceSelectorUI`, et compare le retour à 997
(`ERROR_IO_PENDING`). Une fonction dont la moitié existe en double, atteignable
indépendamment, peut initialiser l'objet deux fois ou partiellement — ce qui
produirait précisément un dialogue **construit mais incomplet** : structure et
boutons présents, texte absent, actions inertes.

Cela relierait en une seule cause les deux symptômes des cycles 336 à 338.

## 3. Test qui tranche, bon marché

Retirer `0x821CE8E0` de `[functions]`, régénérer, reconstruire, rejouer la
traversée :

- si le dialogue affiche son texte, la cause est la frontière parasite ;
- s'il reste vide, la frontière est un défaut réel mais distinct, et la
  recherche du texte reprend dans `sub_821CE8A8` et ses voisines.

Une seule régénération et une reconstruction, sans instrumentation nouvelle.

## 4. Ce qui n'est toujours pas fait

- **Identifiant de chaîne : non journalisé.** Ni `sub_821CE8A8` ni
  `rex_sub_821CE8E0` ne contiennent de recherche de chaîne ; le texte se
  résout ailleurs.
- **Exécutions oracle : non préparées.** Rien lancé. Rappel du cycle 316 :
  Xenia sous Xvfb ne rend rien, il faut un affichage headless accéléré GPU,
  jamais la session de l'opérateur. Profil Xenia existant à préserver.

## 5. État global

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

# AC6 — appel virtuel candidat et provenance amont des records (cycle 174)

Date : 2026-07-18 (Europe/Paris)

## Cible et méthode

Cible canonique AC6 Xbox 360 PAL : `default.xex`, target ID
`ac6-xbox360-pal`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`, base
`0x82000000`, projet Ghidra `ace-combat-6`.

Cette passe utilise le désassemblage XenonRecomp et les scripts Ghidra headless
`FindDirectCallsTo.java`, `ReferencesTo.java` et `DumpRange.java`. Aucun
fichier généré, projet Ghidra ou source natif n'a été modifié.

## Appel virtuel compatible avec le contrat de `0x82102e70`

Dans `sub_822131d0`, le site `0x82213558` prépare :

```text
r3 = *(table + 0x36084)       objet obtenu par index
r4 = r1 + 0x60                sortie vectorielle locale
r5 = r1 + 0x70                sortie vectorielle locale
r6 = r1 + 0x80                sortie scalaire locale
r7 = r30                      record d'entrée A (16 octets)
r8 = r28                      record d'entrée B (16 octets)
r9 = 1                        sélecteur/paramètre
```

Puis le code effectue :

```text
vtable = *(r3 + 0)
target = *(vtable + 0x140)
mtctr target
bctrl                         retour à 0x82213558
```

Le slot `+0x140` d'un address-point candidat `0x8205c9a4` contient
`0x82102e70` (`0x8205cae4`). Ce site est donc **ABI-compatible** avec le
parent NDXR étudié aux cycles 168–173 : mêmes trois sorties `r4/r5/r6`, deux
records `r7/r8` et sélecteur `r9`. La provenance dynamique de l'objet `r3`
et l'identité de son address-point ne sont toutefois pas prouvées ici ; la
liaison au NDXR reste `cross-match`, pas `confirmed`.

## Préparation des records par `sub_822131d0`

Le helper reçoit `r4` et `r5` comme deux pointeurs distincts. Avant le
dispatch, il :

- lit les quatre mots de `r4` aux offsets `0/4/8/12` et les copie dans une
  zone locale de 16 octets ;
- lit `r5+0`, `r5+4`, `r5+8` comme flottants et charge aussi son vecteur de
  16 octets ;
- produit des vecteurs et valeurs locales pour ses propres comparaisons ;
- transmet néanmoins les pointeurs originaux `r4` et `r5` au slot virtuel,
  sans réécrire les records avant l'appel.

Cela relie la provenance observée au cycle 173 : les records consommés par
`0x82102e70` peuvent être des records déjà construits par l'appelant, et non
des zones créées par le parent lui-même. Le cycle 174 ne leur attribue encore
aucune unité ni sémantique gameplay.

## Appelants directs du helper

Le balayage headless retrouve sept appels directs à `sub_822131d0` :

```text
0x8222a2ec
0x82233550
0x8223376c
0x822c66b0
0x822c66f0
0x822d6580
0x822d6790
```

Les voies `0x822c66b0` et `0x822c66f0` sont particulièrement utiles : elles
préservent deux pointeurs de records en `r29/r28`, fournissent un paramètre
de mode (`r7=5` ou `r7=2`), récupèrent un pointeur auxiliaire via
`*(r30+224)+176`, puis appellent le helper. Le helper transforme ensuite ces
entrées en appel virtuel du slot `+0x140`.

Les autres appels directs sont conservés comme producteurs potentiels, mais
leur rattachement au même objet et au même address-point doit rester séparé
jusqu'à une preuve de registre/vtable plus précise.

## Confiance et limites

- `confirmed` : site `0x82213558`, préparation des sept arguments et dispatch
  virtuel `vtable+0x140` ;
- `confirmed` : lecture des quatre mots du record `r4` et lecture vectorielle
  du record `r5` dans `sub_822131d0` ;
- `confirmed` : sept appels directs au helper `0x822131d0` ;
- `cross-match` : `0x8205c9a4 + 0x140 -> 0x82102e70` ;
- `unknown` : address-point réellement utilisé par l'objet `r3`, type C++,
  rôle du sélecteur, unités et sémantique métier des deux records.

Ne pas nommer ces records comme positions, sommets, cellules ou avions. La
prochaine passe utile est de suivre la table `r25+0x36084` et les address-points
réels des objets retournés, sans lancer de run humain.

Aucune action humaine, VNC ou run Xenia n'est nécessaire pour cette
qualification statique.

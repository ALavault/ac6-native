# `DurableBin` — qualification scan du `Default.xex` démo

## Cible et provenance

Cette note porte uniquement sur le `Default.xex` démo, SHA-256
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, dans le
projet Ghidra `ace-combat-6-demo`. La preuve PAL
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` est
documentée séparément dans `durable-bin.md` et ne fournit pas à elle seule le
consommateur du payload de la démo.

## Résultat borné

La chaîne de consommation est maintenant prouvée par Ghidra et reste
volontairement opaque au-delà du premier octet :

```text
runtime+0x2A0
    -> ObjBin*                     (0x82095E98: stw r31,0x2A0(r29))
    -> ObjBin+0x0C / DurableBin*   (0x82095D90)
    -> DurableBin+0x00 / payload
    -> lbz payload+0               (0x82095DC8, 0x8209611C)
```

`0x82095E98` récupère l’entrée par la table parallèle via `0x821EE0F8`,
emploie ses mots `+0x00..+0x1C` comme l’entrée `ObjBin`, puis attache ce
pointeur au runtime à `+0x2A0`. `0x82095D90` recharge ensuite ce même pointeur,
teste `ObjBin+0x0C`, recharge le wrapper, lit `DurableBin+0x00`, puis charge un
octet avec `lbz`. Le lecteur `0x82333738` contient en parallèle l’accès
qualifié `lwz ...,0x0C(...)` du child.

La table parallèle est conservée dans l’ABI produit sous `UnitTblBinAbi` :
`+0x04` pointe vers des `UnitBinAbi` de stride `0x08` et `+0x08` vers des
`ObjBinAbi` de stride `0x20`. Le second mot de `UnitBinAbi` reste opaque ; la
stride ne vaut pas qualification de son contenu.

La largeur et l’offset du premier champ observé sont donc établis :
`uint8_t` à `payload+0`. Cela ne donne ni la longueur du payload ni le nom
sémantique de cet octet, et ne justifie toujours pas `hit_points`, `armor` ou
un tableau de résistance.

La frontière voisine du codegen est désormais qualifiée :
`0x8233372C..0x82333734` est la plage de données exportée, puis l’exécution
reprend à `0x82333738`. Cela ferme une frontière de génération, pas le schéma
du payload.

## Scans et limite de preuve

Le scan direct historique `+0x0C` avec propagation locale stricte a produit
`hits=0`. Le scanner borné plus large a trouvé `3 682` ancres et `475`
candidats syntaxiques. Ces candidats mélangeaient les pointeurs de wrappers,
descripteurs et structures voisines ; la chaîne retenue ci-dessus est la seule
promue parce que la propriété `runtime+0x2A0 -> ObjBin*`, le lookup parallèle et
les deux lectures `payload+0` sont tous vérifiés par
`ExportDurableConsumerEvidence.java` dans le projet Ghidra démo qualifié.

En particulier, un motif `lwz ...,0(rX)` dans une fenêtre locale ne suffit pas
à établir que `rX` est le `DurableBin*` issu de `ObjBin+0x0C`. Il faut suivre la
valeur retournée ou stockée par le lecteur jusqu'au premier chargement
indirect, puis qualifier l'offset, la largeur, l'endianess et le consommateur
runtime. Aucun champ `hp`, `armor` ou tableau AC5 n'est donc exposé.

## Modèle produit retenu

Le produit conserve seulement `DurableBinAbi` (`0x10` octets, adresse invitée
32 bits à `+0x00`) et `resolve_durable_payload`, qui vérifie que cette adresse
reste dans le buffer de mission décodé. La vue retournée est sans longueur et
non propriétaire. Un buffer expiré ou hors plage produit une vue invalide ; il
n'y a ni copie ni succès synthétique.

Le premier lecteur indirect est désormais fermé pour `payload+0` seulement. La
lane mission/objectifs reste ouverte pour le décodage des octets suivants, la
durée de vie en exécution et la transformation en état de survivabilité.

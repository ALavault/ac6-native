# AC6 cycle 205 — gates de transition et copie de l'état normalisé

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- image base : `0x82000000`

Passe statique headless en lecture seule sur le projet Ghidra canonique. Aucun
état Ghidra, binaire ou sortie générée n'a été modifié.

## Gate autour de `0x8226e740`

Dans le chemin de transition, la valeur byte à `record+0x40` est comparée à
`2`. Lorsque le chemin est actif :

- `context+0x0c` est incrémenté ;
- une table externe au slot `0x03006078` est notifiée ;
- lorsque l'état reste `2`, le tableau `context+0x58` est parcouru avec un
  stride de `0x44` ; les records dont `+0x04` portent le bit observé sont
  mis à jour avec `context+0x64` vers `record+0x1c` et reçoivent le bit `0x20`;
- si le champ global de contrôle au slot `0x03006078` vaut `3`, le chemin
  appelle `0x8226b290(context, f1)` puis `0x822663b0(collection, f1)` ;
- le chemin rejoint ensuite un slot virtuel `+0xa8` de l'objet, le
  consommateur `0x8226c388`, puis un dispatch avec les codes `6` ou `7` selon
  `record+0x40`.

Le rôle sûr est une transition bornée par état et par record. Il ne justifie
pas un nom de vol, de caméra ou d'avion.

## Chemin frère autour de `0x8226ea30`

Un autre chemin reçoit un objet en `r27` et :

- parcourt deux tableaux à `object+0x2a0` et `object+0x2a4`, puis appelle
  `0x8226cf90` pour mettre à jour les éléments correspondants ;
- publie `object+0x2b4` dans le slot global `0x0200d7cc` ;
- appelle les services de table au slot `0x03006054` (`+0xd8`, puis `+0x0c`) ;
- appelle `0x8226b290(object, 0.0)` puis `0x82269be0(object, 0)` ;
- parcourt la collection `0x02009fc8` et appelle `0x8224f3c0(entry, 1)` pour
  les entrées dont `entry+0x184` et son byte `+0x19` passent le gate de bit.

La paire `0x8224f3c0(entry,0)` / `0x8224f3c0(entry,1)` constitue donc une
frontière de nettoyage/activation de flags, sans nom métier confirmé.

À la fin, le chemin lit `object+0x268`, suit `+0x14` puis le premier byte du
payload ; `0xff` et `-1` sont ramenés à zéro avant l'appel d'un slot virtuel
`+0x0c` du service global. C'est un sélecteur borné, pas une preuve d'identité
de mission.

## Copieur `0x8226cf90`

Le helper reçoit destination `r3` et source `r4`. Il appelle d'abord le
copieur de sous-bloc `0x8226b368`, puis recopie explicitement les zones
suivantes de `source` vers `destination` :

```text
+0x0d0..+0x164  mots et flottants de l'état local
+0x168           flottant
+0x16c..+0x18c  mots
+0x190           pointeur/slot de sous-structure
+0x194..+0x1a0  flottants
+0x1a4..+0x1ac  flottants et mot de contrôle
+0x1b0..+0x1bc  mots
+0x1c4..+0x1c8  flottants
+0x1cc           mot
+0x1d4..+0x1e0  flottants
+0x1dc           mot
```

La portion pertinente pour les cycles 201–204 est confirmée :
`+0x140/+0x144/+0x148/+0x14c`, `+0x170`, `+0x190` et
`+0x1a4/+0x1a8/+0x1ac` sont transportés ensemble par cette copie. Cela
renforce un contrat de structure offset-qualified, sans attribuer de
sémantique gameplay à ces champs.

## Qualification

- `confirmed` : gates `record+0x40`, stride `0x44`, appels convergents,
  activation/nettoyage des flags, et copie des offsets normalisés ;
- `cross-match` : même cycle de transition/collection que les cycles 199–204 ;
- `unknown` / `needs-dynamic-evidence` : identité fonctionnelle des records,
  services et sélecteurs.

Aucune intervention humaine n'est requise.

## Validation

- `DumpRange.java 0x8226e620 0x8226e7d0` ;
- `DumpRange.java 0x8226e920 0x8226eb20` ;
- `DumpRange.java 0x82266390 0x82266440` ;
- `DumpRange.java 0x8226cf90 0x8226d180` ;
- projet : `workspaces/ace-combat-6/ghidra-projects/ace-combat-6` ;
- mode : `-readOnly -noanalysis`.


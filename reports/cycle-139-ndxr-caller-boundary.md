# AC6 — appelants directs du contrat NDXR `0x822c2148`

Date : 2026-07-17 (Europe/Paris)

## Cible et méthode

Cible : `default.xex` Xbox 360 PAL, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

Cette passe utilise uniquement le projet Ghidra corrigé en lecture headless.
Elle ne lance ni Xenia, ni Wine, ni GUI, et ne modifie pas le projet Ghidra.

Commandes exécutées :

```bash
./.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -readOnly -noanalysis -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript FindDirectCallsTo.java 0x822c2148

./.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -readOnly -noanalysis -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript DumpRange.java 0x82105b80 0x82105d20

./.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -readOnly -noanalysis -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript DumpRange.java 0x82105ec0 0x821060a0

./.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -readOnly -noanalysis -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript DumpRange.java 0x821060a0 0x82106358
```

Le catalogue exporté `exports/822c2148.json` déclare encore `callers: []`.
La recherche instructionnelle directe corrige cette absence sans modifier le
catalogue ni inventer de frontière de fonction.

## Appelants retrouvés

La recherche retourne exactement trois appels directs :

```text
0x82105ccc -> 0x822c2148   bl 0x822c2148
0x82105fb8 -> 0x822c2148   bl 0x822c2148
0x821061c0 -> 0x822c2148   bl 0x822c2148
```

Les trois sites appartiennent au même worker brut commençant à
`0x82105bb8`. Le worker :

- reçoit son contexte dans `r3` et le conserve en `r18` ;
- abandonne si `context+0x28 == 0` ;
- abandonne si `context+0x5c <= 4` ;
- initialise deux tables temporaires de `0x100` entrées (`r1+0x2b0` et
  `r1+0xb0`) ;
- parcourt plusieurs groupes et réutilise le même contrat de ressource pour
  les trois appels ;
- termine en publiant un curseur/compteur à `context+0x30`.

Ghidra ne rattache pas le bloc à une fonction englobante : son catalogue ne
contient qu'un petit stub `0x82105ba8..0x82105baf`, puis le worker brut
`0x82105bb8..0x82106354`. Cette frontière de fonction reste donc à corriger
ou à confirmer avant toute tentative de nom métier.

## Contrat d'appel commun

Avant chaque appel, le worker :

1. charge un élément de table dans `r31` ;
2. extrait un champ de 9 bits (`(r31 >> 16) & 0x1ff`) pour l'appel indirect
   via le slot `+0x5c` de l'objet `context+0x00` ;
3. reçoit dans `r3` le résultat de cet appel indirect et le transmet à
   `0x822c2148` dans `r5` ;
4. transmet dans `r6` la moitié basse de `r31` (`r31 & 0xffff`) ;
5. fournit deux buffers de sortie : trois floats à l'adresse `r3`, et un
   float séparé à l'adresse `r4`.

Le retour de `0x822c2148` est utilisé comme octet de succès : zéro branche
vers le chemin d'abandon de l'élément, une valeur non nulle autorise le
post-traitement des sorties.

Les trois positions de pile sont distinctes mais suivent exactement le même
ABI :

| Site | sortie vectorielle (`r3`) | sortie scalaire (`r4`) |
| --- | --- | --- |
| `0x82105ccc` | `r1+0xa0 .. r1+0xac` | `r1+0x60` |
| `0x82105fb8` | `r1+0x80 .. r1+0x8c` | `r1+0x74` |
| `0x821061c0` | `r1+0x90 .. r1+0x9c` | `r1+0x70` |

Ces offsets sont des buffers locaux du worker, pas des offsets de la ressource
NDXR.

## Post-traitement prouvé

Après chaque appel accepté, le worker :

- relit les composantes produites et la valeur scalaire ;
- leur ajoute deux bases flottantes conservées dans `f30` et `f29` ;
- convertit les résultats en entiers par `fctiwz` ;
- effectue une réduction signée par décalage de `9` bits ;
- borne plusieurs composantes dans l'intervalle `0..0xf` ;
- met à jour des tables temporaires à pas de `0x10`, `0x80` ou `0x8` selon la
  branche.

Cette passe prouve donc un traitement flottant puis une quantification compacte
et une mise à jour d'état/table. Elle ne prouve pas que ces valeurs sont des
coordonnées, des LOD, des paramètres d'avion, des attributs de dessin ou des
états de vol.

## Ce qui reste inconnu

Les trois appelants réduisent l'incertitude sur l'ABI, mais ne ferment pas
encore :

- le type de l'objet derrière `context+0x00` et son slot virtuel `+0x5c` ;
- le type du pointeur retourné par cet appel indirect et passé en `r5` ;
- l'identité des tables temporaires et de leur consommateur final ;
- la relation éventuelle avec un modèle, une scène, un renderer ou le vol ;
- la frontière de fonction englobante autour de `0x82105bb8`.

En conséquence, aucun nom de gameplay n'est ajouté et aucun helper natif n'est
modifié dans cette tranche.

## Confiance

`confirmed` :

- trois et seulement trois appels directs trouvés par le balayage du code ;
- ordre des registres `r3`/`r4`/`r5`/`r6` aux trois sites ;
- tailles et positions des buffers locaux ;
- test du retour par l'octet bas ;
- conversion, décalage et bornage `0..0xf` observés après les appels ;
- worker partagé et préconditions `context+0x28`/`context+0x5c`.

`unknown` :

- sémantique métier des quatre valeurs NDXR ;
- classe/ressource du pointeur `r5` ;
- identité du résultat quantifié ;
- frontière et nom de la fonction englobante.

## Suite statique

La prochaine tranche doit résoudre l'appel indirect `context+0x00` / slot
`+0x5c`, puis suivre le pointeur retourné et les writers des tables quantifiées.
Elle peut rester headless et en lecture seule. Une intervention humaine n'est
pas requise à ce stade ; une session dynamique ne sera demandée que si ces
preuves statiques ne suffisent plus à départager les rôles.

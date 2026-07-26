# AC6 cycle 216 — recheck headless de la frontière du producteur brut

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- projet canonique : `workspaces/ace-combat-6/ghidra-projects/ace-combat-6`
- mode : Ghidra headless, `-readOnly -noanalysis`

Cette passe ne modifie ni projet Ghidra, ni XEX, ni sortie de recompilateur.
Elle revalide explicitement la limite fonctionnelle autour de l'appel déjà
qualifié `0x82127f30 -> 0x82136168`, afin d'éviter de convertir un îlot PPC
en méthode C++ sur la seule base de ses offsets.

## Résultat

`InspectFunctionIsland.java 0x82127b80 0x82128120` ne retourne aucune entrée
de fonction pour l'îlot contenant le site. Le dump PPC confirme seulement la
séquence locale :

```text
0x82127eb0..0x82127ed0 : stores vers base r25 + 0x18c/0x190/0x194/0x1a0/0x1a4/0x1a8
0x82127ed4..0x82127f14 : appels virtuels à slots +0x04 puis +0x34
0x82127f18..0x82127f2c : r3=r25, r4=r28, r5=r27, r6=r26, r7=r30, r8=r29
0x82127f30              : bl 0x82136168
```

La décompilation exportée de `0x82136168` confirme de son côté un contrat
structurel : elle obtient un état via `0x82382edc`, initialise ou sélectionne
des sous-objets, transmet `state+0x1a4/+0x1a8` à des services indirects, copie
les cinq arguments bas vers `state+0x198/+0x199/+0x19a/+0x1ac/+0x1ad`, puis
appelle `0x821365f8`. Les vtables dynamiques, la fonction contenante du
producteur et l'identité de l'état restent absentes.

## Décision

Le bloc n'est pas promu en méthode, constructeur, objet avion, caméra ou
transition de mission. Les faits suivants restent `confirmed` : site d'appel,
ordre des registres d'arguments, offsets écrits localement et callees directs.
Le type de l'objet, les vtables dynamiques et toute sémantique gameplay restent
`needs-types` / `needs-dynamic-evidence`.

Cette revalidation ferme une réanalyse statique redondante : la prochaine
preuve utile doit venir d'une frontière typée indépendante ou d'une trace
runtime bornée, pas d'un nouveau nom pour `0x82127f30`. Elle ne requiert aucune
action humaine tant qu'une telle trace n'est pas explicitement planifiée.

## Validation exécutée

```bash
./.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -readOnly -noanalysis -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript InspectFunctionIsland.java 0x82127b80 0x82128120 \
  -postScript DumpRange.java 0x82127eb0 0x82127f50
```

La commande réussit en lecture seule. Aucun test natif n'est relancé : cette
livraison ajoute seulement une qualification de preuve et aucun code.

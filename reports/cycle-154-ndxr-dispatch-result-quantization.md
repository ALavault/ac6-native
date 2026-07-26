# AC6 — flot de retour du dispatch NDXR et quantification (cycle 154)

Date : 2026-07-17 (Europe/Paris)

## Cible et méthode

Cible : `default.xex` Xbox 360 PAL, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

Cette passe est statique, headless et en lecture seule. Elle réutilise les
contrats des cycles 152–153 et examine les instructions immédiatement après
les trois appels indirects du worker vers `0x822c2148`.

Commande principale :

```bash
./.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -readOnly -noanalysis -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript DumpRange.java 0x82105ca8 0x82105e30 \
  -postScript DumpRange.java 0x82105f90 0x821060f0 \
  -postScript DumpRange.java 0x82106198 0x82106350
```

## Contrat répété des trois sites

Les trois séquences ont la même forme :

```text
dispatch indirect
or      r5,r3,r3
rlwinm  r6,r31,0,16,31
prépare un buffer de trois floats et un float scalaire
bl      0x822c2148
rlwinm  r11,r3,0,24,31
cmplwi  r11,0
```

Les sites sont `0x82105ccc`, `0x82105fb8` et `0x821061c0`. Le résultat du
dispatch virtuel est donc transmis en `r5` à `0x822c2148`; cette observation
ne permet pas encore de distinguer un handle, un pointeur ou une valeur
opaque. Le retour de `0x822c2148` est utilisé uniquement comme un test de
succès sur son octet bas : le chemin zéro abandonne l'élément, le chemin non
nul traite les sorties.

Les buffers locaux sont distincts :

| appel | buffer de trois floats | float scalaire |
| --- | --- | --- |
| `0x82105ccc` | `r1+0xa0..0xac` | `r1+0x60` |
| `0x82105fb8` | `r1+0x80..0x8c` | `r1+0x74` |
| `0x821061c0` | `r1+0x90..0x9c` | `r1+0x70` |

Ces adresses sont des buffers de travail du worker et ne sont pas des champs
de l'objet NDXR.

## Post-traitement confirmé

Après un retour accepté, chaque bloc :

1. relit les trois composantes et le scalaire ;
2. ajoute les bases flottantes conservées dans `f30` et `f29` ;
3. produit des entiers avec `fctiwz`/`stfiwx` ;
4. effectue une réduction signée par `srawi ..., 9` suivie de `addze` ;
5. borne plusieurs résultats à l'intervalle `0..0xf` ;
6. rejette le cas où la borne basse dépasse la borne haute ;
7. met à jour des tables temporaires par incréments de `0x20`, `0x80` ou
   `0x8` selon le bloc et la branche.

Le premier bloc utilise notamment les valeurs temporaires de `r1+0x50`,
`r1+0x54`, `r1+0x58` et `r1+0x5c`, puis écrit dans la zone commençant à
`r1+0xb0`. Le deuxième et le troisième reprennent la même structure avec des
zones de pile différentes. Les mises à jour sont des opérations entières
(`lhz`/`sth` ou `lwz`/`stw`) ; elles ne prouvent pas à elles seules la nature
des données.

## Limites d'interprétation

Cette passe confirme un contrat de dispatch, un test de succès et une étape de
quantification compacte. Elle ne confirme pas que les quatre valeurs sont des
coordonnées, des paramètres de rendu, des LOD, des attributs d'avion ou des
états de vol. Aucun nom de gameplay, prototype public ou helper natif n'est
ajouté sur cette seule base.

La prochaine frontière statique est le suivi des writers et du consommateur
des tables temporaires, ainsi que la publication finale à `context+0x30`.
Une session Xenia ou une intervention humaine n'est pas nécessaire tant que
ces dépendances peuvent être résolues par les exports headless.

## Confiance

`confirmed` : positions des trois appels, ordre des registres, buffers locaux,
test sur l'octet bas du retour, additions flottantes, conversion, décalage,
bornage et mise à jour des tables.

`unknown` : sémantique du résultat passé en `r5`, rôle des tables temporaires,
signification métier des valeurs quantifiées.

# AC6 cycle 202 — cycle de vie de la normalisation et portée des offsets

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- image base : `0x82000000`

Passe statique headless en lecture seule avec `FindMemoryScalarInRange.java`
et `DumpRange.java`. Aucun état Ghidra, binaire ou sortie générée n'a été
modifié.

## Réinitialisation dans le même chemin

Le fragment `0x8226a400` réutilise le même objet que le chemin d'appel de
`0x822667c8` et fournit une borne de cycle de vie utile :

- `r31+0x140` est matérialisé ;
- `+0x148` et `+0x144` reçoivent la constante flottante nulle ;
- les bits de `+0x14c` sont nettoyés avec les mêmes familles de masques PPC
  que dans `0x822667c8` ;
- la branche réinitialise ensuite le contexte global `0x823fb360` :
  `+0x34 = 0`, `+0x10 = 0`, `+0x3c = -1`, `+0x40 = 0` ;
- les valeurs sauvegardées dans `objet+0x1a4/+0x1a8/+0x1ac` sont recopiées
  vers une sous-structure à `objet+0x190` avant les callbacks de service.

Cette co-occurrence de publication, reset et appels dans le même island
confirme que `+0x144/+0x148/+0x14c` sont des champs d'état de l'objet local,
et non seulement une zone temporaire de pile.

## Portée des occurrences d'offset

Le balayage du seul intervalle `0x82260000..0x82270000` trouve également :

- `0x82260e10`, qui matérialise `r3+0x140` mais exécute une longue séquence
  VMX/normalisation vectorielle sur d'autres champs (`+0x20..+0x2c`) ;
- `0x8226cdd0`, `0x8226ce38`, `0x8226ce98` et `0x8226ceb0`, qui lisent
  `+0x14c` d'objets obtenus via `+0x1008/+0x1330` dans une table distincte.

Ces occurrences ne possèdent pas de chaîne de provenance statique vers
`r3+0x140` du helper `0x822667c8`. Elles doivent rester séparées : un offset
commun n'est pas une preuve d'identité de classe ou de structure.

## Contrat consolidé

Pour le sous-système documenté par les cycles 199–202, le contrat minimal est :

```text
object +0x140
  +0x144 : valeur normalisée / valeur de reset
  +0x148 : magnitude absolue ou zéro selon les flags
  +0x14c : bits de signe/suppression et état de contrôle
```

Le contrat est suffisamment précis pour une structure native offset-qualified,
mais pas pour un nom gameplay. Les noms `scalar_magnitude`,
`scalar_normalized` et `scalar_flags` sont acceptables comme rôles provisoires;
`velocity`, `heading` et `aircraft_state` ne le sont pas.

## Qualification et action humaine

- `confirmed` : reset `0x8226a400`, offsets, constantes, masques et chaîne
  vers le contexte global ;
- `cross-match` : même objet de transition/paramètres que `0x822667c8` ;
- `unknown` / `needs-dynamic-evidence` : sémantique gameplay.

Aucune intervention humaine n'est requise. Les occurrences d'offset hors de ce
chemin sont explicitement conservées comme ambiguës plutôt que fusionnées.

## Validation

- `FindMemoryScalarInRange.java 0x82260000 0x82270000` pour `0x140`,
  `0x144`, `0x148` et `0x14c` ;
- `DumpRange.java 0x8226a360 0x8226a500` ;
- `DumpRange.java 0x82260d80 0x82261218` ;
- `DumpRange.java 0x8226cd80 0x8226cf20` ;
- CTest AC6 : gate connu `41/41` ;
- launcher Xenia/Wine : `status=ready`, `release=16e1eb8`,
  `renderer=vulkan`.


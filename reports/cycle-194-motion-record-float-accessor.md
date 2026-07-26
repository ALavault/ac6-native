# AC6 cycle 194 — accès aux valeurs flottantes des records de mouvement

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- image base : `0x82000000`
- projet Ghidra : `ace-combat-6`

Analyse headless, statique et en lecture seule. Aucun fichier de runtime ou de
génération n’a été modifié.

## Accesseur brut `0x821265e8`

Le point `0x821265e8` n’est pas défini comme fonction complète par Ghidra,
mais sa prologue et son appelant sont contigus. Le contrat ABI observé est :

```text
r3 = record source
r4 = index ou paramètre de sous-record
r5 = sortie four-mots (adresse appelante)
```

Le corps inspecte `record+0x14`, décode son type big-endian à `+0x0a`, puis
accepte les mêmes types que le gestionnaire de mouvement :

```text
0x0011 -> 0x82339508 / 0x823456b0
0x8181 -> validation du flag + accès au sous-record
```

Il utilise ensuite `0x82118b30` pour calculer une taille/compteur de contenu et
écrit quatre valeurs flottantes dans la sortie (`r5+0x00`, `+0x04`, `+0x08`,
`+0x0c`). Ces valeurs sont alimentées par le dispatcher appelant et par des
helpers d’éléments encodés; elles sont donc qualifiées `record_float_tuple`,
pas automatiquement comme position, quaternion ou vitesse.

## Appelant `0x8212b8ac`

Dans le dispatcher autour de `0x8212b834`, les codes `0x2011` et `0x3001..0x3004`
sélectionnent des paramètres dans une table. Pour le cas `0x3001` :

1. un élément est résolu par son index et son offset relatif ;
2. un paramètre d’index est calculé (`field+2 - 1`) ;
3. `0x821265e8` extrait le tuple flottant ;
4. les quatre composantes sont recopiées dans une structure globale et un flag
   est activé.

Les cas voisins utilisent d’autres extracteurs (`0x82135d28`, `0x82126760`,
`0x82126938`, `0x82126a60`) sur la même famille de données. Cette répétition
confirme une infrastructure d’accès aux paramètres de records, sans fournir un
nom de système gameplay.

## Helpers et limites

- `0x82339508` accepte seulement les records de type `0x0011` et récupère une
  valeur via `0x823456b0`.
- `0x823456b0` décode un offset d’élément et réintroduit le marqueur de pointeur
  encodé (`0x80000000`).
- `0x82339618` délègue les pointeurs encodés à un slot virtuel `+0x10` d’un
  objet interne; son implémentation finale reste dynamique.
- `0x82118b30` compte des éléments de sous-records selon leurs tags `1..4`.

La chaîne est donc :

```text
MissionAircraft
  -> record de mouvement/resource
  -> types 0x0011/0x8181
  -> accessibilité de paramètres
  -> tuple de quatre flottants
  -> dispatcher de paramètres 0x3001..0x3004
```

Elle ne démontre toujours pas que la structure globale destination représente
la pose de l’avion joueur, ni qu’elle atteint directement les champs
`CAce6UnitPlayer`, la caméra ou le modèle de vol. Statuts :

- `confirmed` : format des entrées, types, ABI de l’accesseur, quatre stores
  flottants et dispatcher appelant ;
- `cross-match` : interprétation comme paramètre de mouvement ;
- `unknown` / `needs-dynamic-evidence` : sémantique des quatre composantes et
  propriétaire gameplay.

## Suite et action humaine

La prochaine piste statique est de suivre les lecteurs de la structure globale
mise à jour par le cas `0x3001` et de comparer les autres extracteurs voisins.
Aucune intervention humaine n’est requise; une trace runtime ne deviendra
utile que pour nommer les composantes ou relier la structure à un avion précis.


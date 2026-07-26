# AC6 cycle 191 — le slot `ResourceManager+0x14` normalise des records

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- image base : `0x82000000`
- projet Ghidra : `ace-combat-6`

Analyse headless, statique et en lecture seule. Aucun Xenia, Wine, GUI, VNC,
writer Ghidra, XEX ou fichier généré n’a été modifié.

## Call-sites du slot `+0x14`

La vtable statique du service `0x820674d8`, déjà reliée au global
`0x826a0728`, pointe à `+0x14` vers `0x821c14a0`. Le cas
`0x8218c810` appelle cette entrée cinq fois :

```text
0x8218cad8 -> 0x821c14a0
0x8218cb04 -> 0x821c14a0
0x8218cb30 -> 0x821c14a0
0x8218cb5c -> 0x821c14a0
0x8218cba4 -> 0x821c14a0
```

Les cinq séquences ont la même forme :

```text
r3 = service global
r4 = résultat d’une résolution de nœud/entrée
r5 = vue ou résultat de table
call service->vtable[0x14]
```

Le littéral ou hash `MissionAircraft` n’est donc pas transmis directement à
la méthode. Il sert d’abord à résoudre des nœuds et des vues de ressources.

## Contrat brut de `0x821c14a0`

Le point est compiler-split : `0x821c14a0` contient le prologue partagé, puis
le corps commence à `0x821c14a8`. Le corps :

1. conserve `r5` comme vue/résultat entrant ;
2. si `r4` est non nul, appelle le fragment `0x821c1560` ;
3. si la vue `r5` est nulle, sort ;
4. construit un descripteur temporaire sur la pile (`+0x50..+0x70`) ;
5. parcourt la vue via `0x82234dd0` avec une progression contrôlée par le
   compteur au champ `+0x02` ;
6. lit le type du record au byte `record+0x10` ;
7. pour les records admis, appelle `0x823330f0` afin de normaliser les
   offsets/pointeurs et leurs marqueurs de représentation.

Le fragment `0x821c1560` utilise une table de huit branches pour les valeurs
de type observées après soustraction de `5`, puis effectue la même préparation
de descripteur et la même traversée bornée. Les corps traitent des records de
payload ; ils ne contiennent pas de mise à jour directe des champs
`+0x50/+0x54/+0x58` des objets produits par l’entrée 9.

## Limite sémantique

La chaîne statique est maintenant :

```text
ResourceManager::MissionAircraft
  -> CRC-32
  -> nœud hiérarchique
  -> vue/entrée de ressource
  -> slot service +0x14
  -> normalisation de records et offsets
```

Elle ne démontre pas :

- une instance `CAce6UnitPlayer` ;
- un spawn ou une position ;
- un état de vol ;
- une caméra de gameplay ;
- un appel au writer de collection `0x8226f050`.

Les noms `resource_node`, `resource_view` et `record_type` restent donc la
qualification correcte. `MissionAircraft` doit rester un identifiant de
registre de ressources, pas un nom de propriétaire runtime.

## Décision et suite

`confirmed` : cible du slot `+0x14`, cinq call-sites, ordre ABI `r3/r4/r5`,
traversée bornée et normalisation offset/pointeur.

`unknown` / `needs-dynamic-evidence` : type concret de la vue, contenu du nœud
retourné, consommateur final et jonction avec l’owner avion/caméra.

La prochaine piste statique est de suivre les producteurs des vues passées en
`r5` et les lecteurs des records normalisés. Aucune action humaine n’est
nécessaire pour continuer ce front.


# AC6 cycle 190 — `MissionAircraft` est une clé de ressource, pas encore l’avion actif

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- image base : `0x82000000`
- projet Ghidra : `ace-combat-6`

Passe headless, statique et en lecture seule. Aucun Xenia, Wine, GUI, VNC,
writer Ghidra ou fichier généré n’a été modifié.

## Dispatcher d’état brut

Le point `0x8218c238` est représenté par Ghidra comme un thunk de quelques
instructions, mais son flux brut contigu réalise un dispatcher sur le champ
`this+0x0c` : les valeurs supérieures à `4` sortent par `0x8218cca0`, et les
valeurs `0..4` utilisent la table de cinq entrées à `0x8218c26c` :

| valeur `this+0x0c` | entrée brute |
|---:|---:|
| 0 | `0x8218c280` |
| 1 | `0x8218c634` |
| 2 | `0x8218c810` |
| 3 | `0x8218cca0` |
| 4 | `0x8218c750` |

Les entrées sont des pointeurs de code dans l’image, non des valeurs de
configuration. Cette table ferme la portée du dispatcher mais ne nomme pas les
cinq états.

## Construction de la clé `MissionAircraft`

Dans le cas `0x8218c810`, le corps prépare plusieurs objets de service par des
dispatchs virtuels sur le sous-objet `this+0x270`. Il forme ensuite :

```text
format string : 0x82067bf8 = "ResourceManager::%s"
argument      : 0x82063438 = "MissionAircraft"
```

Le helper `0x821d1c68` construit cette chaîne dans un tampon local. Son appel
à `0x821d0ef0` applique ensuite le CRC-32 MSB-first avec le polynôme
`0x04c11db7` et le complément final. Le retour du calcul reste dans `r3` et
est immédiatement transmis comme clé à `0x821d2fc0`.

`0x821d2fc0` est un résolveur hiérarchique :

1. il tente la recherche directe sur la liste `container+0x1c` via le slot
   virtuel `+0x04` de chaque nœud ;
2. si elle échoue, il descend dans les enfants dont le slot `+0x0c` autorise
   la récursion ;
3. il retourne le premier mot du nœud trouvé, ou zéro.

La séquence est donc :

```text
"MissionAircraft"
  -> "ResourceManager::MissionAircraft"
  -> CRC-32
  -> recherche hiérarchique dans un registre de ressources
```

Le cas poursuit ensuite des dispatchs de service, notamment au slot `+0x14`,
avec les résultats de cette résolution et des vues de tables. Aucun store vers
les champs `+0x50/+0x54/+0x58` des objets de l’entrée 9, aucun appel direct au
writer `0x8226f050` et aucune écriture de position/état de vol n’apparaissent
dans la tranche inspectée.

## Décision de preuve

`confirmed` :

- dispatcher cinq états et ses bornes brutes ;
- format exact de la clé `ResourceManager::MissionAircraft` ;
- chaîne CRC-32 et passage de la clé au résolveur ;
- contrat de recherche hiérarchique et retour du nœud ;
- séparation entre ce registre de ressources et les champs de l’unité d’entrée
  9.

`unknown` / `needs-dynamic-evidence` :

- classe concrète du nœud retourné ;
- valeur runtime de la ressource `MissionAircraft` ;
- lien éventuel entre cette ressource et un objet `CAce6UnitPlayer` ;
- consommateur final dans le modèle de vol ou la caméra.

Le nom textuel `MissionAircraft` ne doit donc pas être promu comme identité de
l’avion contrôlé. Cette passe réduit un faux chemin d’interprétation et permet
de suivre proprement le nœud retourné ou les méthodes de service suivantes.

## Suite sans action humaine

Suivre statiquement les résultats des dispatchs `+0x14` du cas
`0x8218c810`, en conservant `resource_node`/`resource_key` comme noms
offset-qualified. Une session humaine ou Xenia ne devient nécessaire que si le
nœud retourné ne peut plus être relié statiquement à un owner, une caméra ou un
consommateur de vol.


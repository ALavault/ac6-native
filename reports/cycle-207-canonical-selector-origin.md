# AC6 cycle 207 — origine canonique du sélecteur `1`

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- projet : `workspaces/ace-combat-6/ghidra-projects/ace-combat-6`
- mode : Ghidra headless, `-readOnly -noanalysis`.

Cette passe revalide sur l'image canonique les deux producteurs de valeur
`current-level` qui avaient été décrits depuis le projet historique. Aucun
résultat du projet `ace-combat-6-corrected` n'est utilisé.

## Incrément de progression `0x821a6048`

Le corps canonique de `0x821a6048` :

1. lit le global propriétaire `DAT_8293BA10` et écrit un pointeur de service à
   `owner+0x10`;
2. appelle le slot virtuel `+0x34` avec l'événement `0x109`;
3. lit l'état runtime au slot `0x03006078 + 0x78`;
4. lorsque cet état vaut `1`, lit le current-level via `0x820943b0` sur
   `global+0x70`;
5. si la valeur est inférieure à `15`, appelle le setter `0x82196508` avec
   `current_level + 1`;
6. sinon, écrit `0` et marque l'octet `record+0x2b4` à `1`.

Le cas `stored level = 0 -> selector = 1` est donc confirmé sur le binaire
canonique. Le corps ne lit aucun FHM, DPL id, index `DATA.TBL`, chaîne de Scene
ou identifiant de groupe.

## Callback événement `0x821b6258`

Le corps canonique filtre :

```text
event_id == 0x259
0 <= value <= 7
```

Il transmet `value` à `0x82196508(global+0x70, value)`, puis, pour les valeurs
`1..7`, calcule un record dans un tableau de stride `0xa7e0` et transforme
uniquement un état de record de `1` vers `2`. Les références de données
canonique vers le callback sont :

```text
0x8206573c  0x8206729c  0x8206734c
```

Ces tables contiennent des pointeurs de callback; elles ne portent pas de
texte UI, de DPL id, de `DATA.TBL` index ou de chemin Scene. Aucun appel direct
canonique ne relie ce callback à une interface de campagne.

## Consommateur de la valeur

Le chemin canonique `0x820a85e0` appelle directement `0x821b6e58` après avoir
lu le current-level sur `global+0x70`, puis formate et demande la ressource DPL.
Cette jointure reste :

```text
current-level selector 1
 -> 0x821b6e58
 -> DPL id 9 / DPL::[0x9,0]
 -> DATA.TBL entry 9
```

Elle établit la ressource chargée, mais pas le choix d'un groupe Scene
particulier dans cette ressource. Les stores et appels du producteur ne
contiennent aucune donnée reliant `selector 1` à `22.1.0`, à un autre groupe,
à `CutTerminate` ou à un objet de vol.

## Qualification et limite

- `confirmed` : production canonique de `selector 1` par l'incrément d'état,
  écriture événementielle bornée `0x259`, et consommation par la route DPL;
- `cross-match` : même domaine current-level que les rapports antérieurs;
- `unknown` / `needs-dynamic-evidence` : contrôle UI réel, mapping vers un
  groupe Scene, et consommation post-CUT.

Cette tranche ne ferme donc pas le verrou post-CUT et ne justifie ni nouvelle
route native ni run humain. La prochaine piste statique est le consommateur
du cycle NFIC `CutTerminate` ou une activation post-DPL qui lit effectivement
un record Scene précis.

## Validation

- `DumpRange.java 0x821a5f00 0x821a6200`;
- `DumpRange.java 0x821b6100 0x821b6400`;
- `DumpRange.java 0x821b6e00 0x821b6f80`;
- `DumpRange.java 0x821b6f7c 0x821b7040`;
- `ReferencesTo.java` pour `0x821a6048`, `0x821b6258`, `0x821b6e58`;
- tous sur le projet canonique, lecture seule.

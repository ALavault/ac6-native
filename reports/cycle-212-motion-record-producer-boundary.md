# AC6 cycle 212 — provenance des records de mouvement

## Cible et méthode

- target : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- projet : `workspaces/ace-combat-6/ghidra-projects/ace-combat-6`
- mode : Ghidra headless, `-readOnly -noanalysis`

Cette passe vérifie les producteurs et consommateurs statiques des deux tags
dispatchés par le `CX360MotionRequestManager` (`0x11` et `0x8181`). Elle ne
modifie ni le projet Ghidra ni les sorties générées.

## Appels directs vers les helpers de records

La recherche headless `FindDirectCallsTo.java` trouve :

```text
82118a80 -> 82339718
82118ae0 -> 82119740
8211bcd4 -> 82118a50
8211bd78 -> 82339508
8211bf5c -> 82119740
82125b3c -> 82339508
82125cf8 -> 82339508
82126034 -> 82339508
8212612c -> 82339508
821263a4 -> 82339508
82126628 -> 82339508
8212679c -> 82339508
82126974 -> 82339508
82126a9c -> 82339508
82127244 -> 82339508
8212747c -> 82339508
82133f9c -> 82339508
821344c0 -> 82339508
82135d68 -> 82339508
```

Les appels vers `0x82339508` appartiennent à plusieurs îlots PPC bruts du
parseur de records. Ghidra ne leur attribue pas encore de limites de fonction;
ils ne doivent donc pas recevoir de noms métier.

La décompilation de `0x82118a50` établit seulement le contrat suivant :

- tag `record+0x0a == 0x11` : délégation à `0x82339718` ;
- tag `record+0x0a == 0x8181` : matérialisation de la table relative
  `record+0x1c`, normalisation de chaque élément de stride `0x20` et pose du
  bit `0x80000000` dans `record+0x18` ;
- le retour est le compteur `record+0x14` ou zéro.

`0x8211bd50` lit ensuite cette table normalisée pour le tag `0x8181`, ou
transmet le tag `0x11` à `0x82339508`. Aucun de ces contrats ne porte un
identifiant de mission, d'appareil ou de caméra.

## Quatre sites de sélection de ressources

Les appels directs `0x82128c90`, `0x8212a100`, `0x8212a23c` et `0x8212a598`
vers `0x82118a50` ont la même forme PPC :

1. appeler `0x8212c020` pour résoudre un index dans une table de ressources ;
2. charger `entry+0xd0` ;
3. écrire ce pointeur dans un objet local aux offsets `+0x14`, `+0x1c`, `+0x3c`
   ou `+0x24` ;
4. normaliser le record par `0x82118a50` ;
5. poser le global `0x826948c0` à `1`.

Le bloc `0x82128c90..0x82128ca8` est représentatif : après la résolution de
`entry+0xd0`, il stocke le pointeur au champ `+0x14`, appelle le normaliseur,
puis pose `0x826948c0 = 1`. Les trois autres sites répètent la même opération
sur des champs différents. Cette preuve qualifie un assemblage de
records/ressources; elle ne prouve pas que ces champs sont des requêtes de vol.

## Producteur brut `0x82127f30`

`FindDirectCallsTo.java` confirme l'unique appel direct qualifié :

```text
82127f30 -> 82136168 bl 0x82136168
```

Le bloc PPC voisin écrit `+0x18c`, `+0x190`, `+0x194`, `+0x1a0`, `+0x1a4` et
`+0x1a8`, puis appelle `0x82136168`. La fonction contenante et le type de
l'objet restent non récupérés.

La décompilation de `0x82136168` montre une initialisation d'objet et de
sous-objets : création via `0x82382edc`, initialisation de sous-états à
`+0x14/+0x18`, résolution d'une valeur par `0x822c47c0`, appels virtuels sur
un sous-objet et transmission ultérieure des champs `+0x1a4/+0x1a8`. Elle ne
montre ni selector campagne, ni `CutTerminate`, ni objet joueur/aéronef typé.

Les appels indirects aux slots observés à `0x8213653c`, `0x82136560`,
`0x821368f0`, `0x82136914` et `0x822ef000` ne peuvent pas être attribués à un
type métier sans résoudre leurs vtables dynamiques. Ils restent
`needs-types`/`unknown`.

## Conclusion

Cette passe renforce la séparation suivante :

- `0x82118a50`/`0x8211bd50` : normalisation et lecture de records taggés ;
- `0x82128c90`, `0x8212a100`, `0x8212a23c`, `0x8212a598` : sélection et
  assemblage de pointeurs de ressources dans des objets locaux ;
- `0x82127f30 -> 0x82136168` : producteur d'un contexte objet dont le type et
  la signification gameplay sont encore inconnus ;
- `CX360MotionRequestManager` : consommateur confirmé des tags, mais pas encore
  relié statiquement au receiver campagne ou à un propriétaire de vol.

Aucun joint nouveau ne relie ces sites au receiver `DAT_8293BA10 + 8`, au
mapping campagne du selector `1`, à un consommateur typé de `CutTerminate` ou à
un objet de vol/caméra. La frontière AC6 reste donc `scene_complete` et l'état
reste `native-partial`. Cette absence de preuve est statique; aucune action
humaine ni session VNC n'est requise pour ce cycle.

Une nouvelle exécution de `TraceGlobalReceiverDispatches.java 0x8293ba10 0x8`
retrouve 14 dispatches `owner -> owner+0x8 -> vtable+0x20`, tous avec
`r4=3`. Aucun site ne recouvre les îlots de records ou le producteur
`0x82127f30`; le chemin receiver reste donc séparé.

## Validation

- `FindDirectCallsTo.java` sur `0x82118a50`, `0x8211bd50`, `0x82339718`,
  `0x82339508`, `0x82119740`, `0x82136168` et `0x821365f8` ;
- `DecompileMany.java` sur les normaliseurs, le dispatcher et `0x82136168` ;
- `DumpRange.java` sur les quatre sites d'assemblage de ressources et la zone
  `0x82136380..0x82136a00` ;
- toutes les opérations sur le projet PAL canonique en lecture seule.
- `TraceGlobalReceiverDispatches.java 0x8293ba10 0x8` : 14 dispatches
  canoniques, sans recouvrement avec les sites de records.
- `ctest --test-dir .build/ace-combat-6/native --output-on-failure -j16` :
  **41/41 PASS**.
- `workspaces/ace-combat-6/scripts/launch_xenia_ac6_wine.sh check` :
  `status=ready`, `release=16e1eb8`, `renderer=vulkan`,
  `service=ac6-xenia-wine-gui.service`.

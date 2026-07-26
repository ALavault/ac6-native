# AC6 — balayage des lecteurs du workspace publié (cycle 157)

Date : 2026-07-17 (Europe/Paris)

## Cible et méthode

Cible : `default.xex` Xbox 360 PAL, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

La passe reste headless, en lecture seule et sans exécution Xenia. Elle cherche
les instructions qui utilisent le scalaire `0x30` autour de la famille NDXR,
puis inspecte les slots vtable déjà qualifiés. Un offset numérique seul ne
constitue pas une identité de champ.

Pour éviter un cache de compilation de scripts Ghidra périmé après ajout des
helpers, les commandes de lecture ont utilisé un répertoire temporaire ne
contenant que le script invoqué :

```bash
scriptdir=$(mktemp -d)
cp workspaces/ace-combat-6/scripts/FindMemoryScalarInRange.java "$scriptdir/"
./.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -readOnly -noanalysis -process default.xex \
  -scriptPath "$scriptdir" \
  -postScript FindMemoryScalarInRange.java 0x820f0000 0x82130000 0x30
```

Les plages autour des slots NDXR ont aussi été exportées avec `DumpRange.java`.

## Résultats

### Slots de la vtable NDXR

À partir de l'address-point `0x8205c9a4`, le dump confirme :

```text
+0x7c -> 0x821005e8
+0x80 -> 0x82100600
+0x84 -> 0x82100628
```

Leurs contrats statiques sont :

- `0x821005e8` lit `owner+0x74`, puis retourne `owner+0x6d8c` si la limite
  n'est pas nulle ;
- `0x82100600` lit `owner+0x74` et indexe la table relative à `owner+0x6d8c`
  avec le `hi9` borné ;
- `0x82100628` retourne `owner+0x28`.

Aucun de ces slots ne lit `owner+0x30`. Le slot `+0x10c` (`0x820fbc28`) et la
méthode `+0x110` (`0x820fa9c0`) ne contiennent pas non plus de lecture directe
de ce champ dans les plages exportées.

### Faux positif de dispatch virtuel

Le candidat proche `0x820f85f8` est :

```text
0x820f85f0  lwz  r11,0x0(r3)
0x820f85f8  lwz  r11,0x30(r11)
0x820f85fc  mtspr CTR,r11
0x820f8600  bctr
```

Cette séquence charge d'abord le premier mot de l'objet, puis lit l'entrée
`vtable+0x30` pour un dispatch indirect. Elle ne lit pas `object+0x30` et ne
fournit donc aucune preuve sur le workspace publié par `0x82106344`.

### Autres occurrences du scalaire

Le balayage `0x820f0000..0x82130000` retrouve plusieurs `lfs/stfs/lwz/stw` à
`+0x30` dans d'autres sous-systèmes (`0x82118b30`, `0x82119740`,
`0x82121038`, etc.). Aucun n'est relié à l'address-point `0x8205c9a4`, au
receiver conservé par le worker ou à son vtable. Ils restent donc des
coïncidences d'offset, classées `cross-match`/`unknown`.

## Décision de preuve

`confirmed` :

- le worker publie le pointeur de zone à `context+0x30` ;
- les slots NDXR déjà identifiés consomment `+0x74`, `+0x6d8c` et `+0x28`, pas
  `+0x30` ;
- `0x820f85f8` est un accès `vtable+0x30`, distinct d'un accès au champ de
  l'objet ;
- aucune lecture directe du champ publié n'est trouvée dans la famille NDXR
  inspectée.

`unknown` :

- un consommateur situé hors de cette famille ou utilisant une adresse calculée
  indirectement ;
- la durée de vie et le destructeur de la zone ;
- la signification métier du contenu de la zone.

La limite restante est donc une provenance statique incomplète, pas un blocage
humain. Une session Xenia ne sera utile que si un consommateur indirect doit
être suivi dynamiquement après épuisement des liens statiques.

## Validation documentaire

- `FindMemoryScalarInRange.java` sur `0x820f0000..0x82130000` : PASS ;
- `DumpRange.java` sur `0x821002a0..0x82100640` : PASS ;
- `DumpRange.java` sur `0x820fbca0..0x820fc100` : PASS ;
- `DumpRange.java` sur `0x820f83b0..0x820f8620` : PASS ;
- aucun projet Ghidra, XEX, sortie générée ou runtime modifié : PASS.


# AC6 démo PAL — qualification sémantique statique des shaders

## Résultat

L'archive externe `ac6-pal-shader-identification-20260816.tar.zst` est intègre
et sa cartographie sémantique principale a été reproduite depuis les fichiers
PAL locaux, sans exécuter le guest et sans importer ses microcodes dans le
dépôt.

- archive SHA-256 : `7294a028…36a3c` ;
- toutes les entrées de `SHA256SUMS` passent ;
- une extraction fraîche de l'entrée `DATA.TBL` 163 donne 49/49 enfants NSXR,
  sans note de parse ;
- l'extracteur externe régénère 3 802 enregistrements canoniques et 1 891
  microcodes uniques ;
- le TSV canonique régénéré est octet-identique à celui de l'archive, SHA-256
  `32da4493…9082c` ;
- aucun enregistrement canonique ne reste ambigu.

Cette reproduction élève les chemins UPDB et leurs composantes de famille au
rang de preuve statique démo. Elle ne qualifie pas automatiquement le
comportement mathématique suggéré par chaque nom.

## Corpus sémantique

| Élément | Valeur | Statut |
|---|---:|---|
| Chemins UPDB uniques | 1 876 | `demo-qualified` |
| Microcodes VS uniques | 884 | `demo-qualified` |
| Microcodes PS uniques | 1 007 | `demo-qualified` |
| Enregistrements Map/terrain | 108 | `demo-qualified` |
| Microcodes Map/terrain uniques | 78 | `demo-qualified` |
| VS / PS Map uniques | 50 / 28 | `demo-qualified` |
| Vus / non vus par les runs Xenia externes | 6 / 72 | `external-observed` |

Les appartenances ne sont pas exclusives : 37 microcodes portent `Map`, 37
`Map_HDR`, 14 `Ocean` et 28 `MPARTS`. Les variantes lexicales comme `Detail`,
`ExpFog` ou `ProjPos` sont `name-derived` tant que leurs instructions et leurs
constantes ne sont pas qualifiées séparément.

Trois paramètres sont directement présents dans les NSXR fraîchement extraits :

- `ACE_vMapParam` : 40 occurrences ;
- `ACE_vMapMeshSpecularCol` : 14 occurrences ;
- `ACE_vMapPartsSpecularCol` : 36 occurrences.

## Jointure des quatre shaders atteints

| Shader brut | Offset basefile | SHA swap32 | NSXR PAC | Sémantique actuelle |
|---|---:|---|---|---|
| `099625f3…21e4e3` VS | `0x13E20` | `fcb772a9…a390c` | absent | inconnue |
| `4913603d…c98e25` PS | `0x13E80` | `f9b2db9e…d0d79c` | absent | inconnue |
| `93488cb9…a0402b` VS | `0x140A0` | `6e8911a2…b8a25` | absent | pass-through, analyse externe |
| `586168ec…a83cc0` VS | `0x14140` | `5c3f2841…40483` | absent | inconnue |

L'archive retrouve les trois premières identités dans sa table de recherche de
basefile. Son affirmation que `586168…` est absent est un faux négatif : la
plage PAL `0x14140..0x1417B` produit exactement ce hash brut et le hash swap32
ci-dessus. La recherche externe dépendait du corpus de shaders Xenia fourni et
ne constituait pas un balayage exhaustif de toutes les tailles possibles de la
basefile.

Le rôle pass-through de `93488c…` reste `external-analysis` jusqu'à publication
d'un reçu d'instructions Xenos borné. Les trois autres rôles restent inconnus ;
le SPIR-V validé seul ne permet pas de leur attribuer un rôle AC6.

## Frontière et prochain checkpoint

Le catalogue statique peut maintenant alimenter un explorateur de shaders avec
stage, taille, SHA-256, NSXR, chemin UPDB, familles et statut de reachability.
Les microcodes, désassemblages, IR et SPIR-V doivent rester sous `TMPDIR`.

Checkpoint recommandé : traduire fail-closed les 78 shaders Map/terrain uniques,
publier uniquement les diagnostics et interfaces, puis générer pour chaque
shader une fiche métadonnées + CFG. Commencer par les 6 vus par Xenia afin de
recouper l'ABI, puis les 72 statiques non couverts. Une image ne devient preuve
AC6 qu'après jointure avec fetches, constantes, textures et état du draw.

Reçu durable : `analysis/demo/ac6-demo-static-shader-semantics-v1.json`.
Aucun actif propriétaire, microcode, SPIR-V ou désassemblage n'est suivi.

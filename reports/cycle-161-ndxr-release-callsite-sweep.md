# AC6 — balayage des call-sites de libération autour de NDXR (cycle 161)

Date : 2026-07-17 (Europe/Paris)

## Cible et méthode

Cible : `default.xex` Xbox 360 PAL, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

`FindDirectCallsTo.java` et `FindPpcBranchesTo.java` ont recensé les appels
vers `0x82222f20`. Les call-sites de l'île NDXR et de ses sous-objets ont
ensuite été exportés avec `DumpRange.java`. Cette passe reste headless et en
lecture seule.

## Résultats ciblés

| Site | Valeur passée en `r4` | Effet observé | Relation avec `context+0x30` |
|---|---|---|---|
| `0x820f9684` | `this+0x9dc` | libération conditionnelle puis remise à zéro de `+0x9dc`, `+0x9d8`, `+0x920`, `+0x118` | distincte |
| `0x820f9900` | `this+0x9dc` | même famille de nettoyage | distincte |
| `0x820fc95c` | `r30` issu de `0x82382f70` | résultat temporaire d'un chemin de parsing | indirecte, non démontrée |
| `0x820fca80` | `r3` issu d'un appel précédent | libération d'un résultat auxiliaire | indirecte, non démontrée |
| `0x82107840` | `subobject+0x4` | destruction d'un sous-objet après changement de vtable | distincte |
| `0x82107854` | `subobject+0x8` | destruction d'un second sous-objet | distincte |

Les call-sites `0x82107840` et `0x82107854` utilisent le même schéma de
service mais sur un objet dont le vtable est écrit à `0x82107834`; ils ne
concernent pas le receiver NDXR `0x8205c9a4`.

Le balayage global contient de nombreux appels supplémentaires à
`0x82222f20`, mais aucun ne peut être rattaché au champ publié sans suivre le
receiver, le registre d'entrée et le vtable. Un offset `0x30` trouvé ailleurs
reste donc une coïncidence jusqu'à preuve contraire.

## Décision de preuve

`confirmed` :

- les call-sites NDXR inspectés passent des allocations `+0x9dc`, des résultats
  temporaires ou des sous-objets, pas `context+0x30` directement ;
- `0x82222f20` ne peut pas encore être déclaré comme le libérateur du workspace
  publié par `0x82106344` ;
- les champs `+0x30` homonymes d'autres objets ne sont pas une preuve de
  consommation du workspace NDXR.

`unknown` :

- éventuel passage indirect de `context+0x30` à un call-site hors de l'île
  inspectée ;
- ownership et destructeur final de la zone publiée ;
- rôle métier du contenu de la zone.

La session humaine reste inutile. La prochaine passe statique doit rechercher
les chaînes de données qui conservent le pointeur publié ou les méthodes du
vtable NDXR qui chargent une adresse calculée avant un appel à
`0x82222f20`.

## Validation documentaire

- `FindDirectCallsTo.java 0x82222f20` : PASS ;
- `FindPpcBranchesTo.java 0x82222f20` : PASS ;
- `DumpRange.java` sur `0x820f95c0..0x820f9a40` : PASS ;
- `DumpRange.java` sur `0x820fc8c0..0x820fcb00` : PASS ;
- `DumpRange.java` sur `0x82107740..0x82107900` : PASS ;
- aucune écriture Ghidra/XEX/générée/runtime : PASS.

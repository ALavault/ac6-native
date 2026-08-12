# Cycle 1572 — projection contrôleur interrégion AC6 v4

Date : 2026-08-12

## Résultat

La famille Python fail-closed accepte maintenant un seul chemin interrégion :

`default.xex` NTSC-U/J qualifié par son identité exacte → runtime natif PAL.

Le replay brut porte le schéma `ac6.controller-input-replay.v4`, le census
`ac6.controller-cadence-census.v2` et le reçu
`ac6.native-controller-projection-receipt.v4`. La sortie binaire reste
strictement `AC6RTPLY` version 3. Le chemin PAL historique reste en schémas v3,
v1 et v3 respectivement, sans changement d'octets.

## Contrat scellé

- La cible brute v4 ne contient aucun marqueur. Ses dix champs identifient
  exactement le XEX NTSC-U/J `6eefba42...cbbbc` (`media_id=531C30BE`, version
  `v0.0.0.8`, point d'entrée `821F5ED0`, masque `0000FDFF`).
- `sync.marker_contract` est distinct et scelle la fonction `821CA940`, son RVA
  `001CA940`, ses 328 octets et leur SHA-256 `a4c027fc...390d`.
- Le census v2 répète exactement cette cible et ce contrat de marqueur, puis
  reste lié au producteur, à la configuration, au replay parent, au payload et
  à la fenêtre de marqueurs.
- Le reçu v4 sépare `source.oracle.{target,marker_contract}` de
  `native_target`, fixé au XEX PAL `acc302c1...bcde` et au media ID `0379EFB3`.
- La projection 30→60 applique un unique zero-order hold de facteur 2 ; le
  ratio 60→60 reste une identité. Un binaire déjà projeté, une seconde découpe
  ou un contrat de cadence modifié est refusé.

Le manifeste d'identité durable écrit `module_xxh3` en minuscules. Le format
v4 choisit les 16 chiffres hexadécimaux majuscules
`892639B654015428`. La valeur numérique est identique ; il ne s'agit pas d'une
nouvelle mesure.

## Frontière de confiance

Le reçu v4 n'embarque volontairement pas le `producer` brut. Il scelle les
hashes du raw, du payload, du parent et du census ainsi que l'identité
oracle/cible, mais un consommateur qui ne rouvre pas les sidecars ne peut pas
vérifier `implementation_commit`, `binary_sha256` ou `build_sha256`.

Cette limite est acceptable uniquement au niveau
`integrity_only_runtime_census` : le préflight doit conserver
`source_lineage_verified=false`. Le reçu n'atteste ni authenticité du runner,
ni proximité retail PAL, ni parité gameplay. Aucune lane n'est fermée par ce
contrat.

## Garde de non-régression

La fixture synthétique 30 Hz, avec un digest cache composé de 64 `a`, scelle :

| artefact | octets | SHA-256 |
|---|---:|---|
| raw v4 parent | 3 736 | `aa3410b8e4534cf1b0f3e13000d70719d796a103e308821e14585eaf0909fa20` |
| census v2 | 1 847 | `69f7f8cfe174765cd97e94df195f0fab4c73491ae4c2496542d5b43be0680590` |
| raw v4 fenêtré | 4 025 | `48022a3ad178779bcdae55993c011796296e55c76ea3e87edfb7ca8bbfb417ad` |
| AC6RTPLY v3 | 157 | `f8ab11a5885f9a069f98a42f20d1978b26299ece1aaf77082b742103094bcb7b` |
| reçu v4 canonique | 2 835 | `cd8f1b031382e6ba33fb4bdd498f0989fe4a5044cbcc22f72afc00c5533e4482` |

Les tests mutent chaque champ de la cible et du marqueur NTSC-U/J, croisent
les versions raw/census, tentent les deux directions v3↔v4 et vérifient le
refus du double-ZOH. Les quatre hashes historiques de la fixture v3 restent
figés, dont le reçu v3 `d5c911db...fe58`.

Validations locales : 58 tests pytest ciblés, Ruff, format Ruff,
`py_compile` et `git diff --check`.

## Risques résiduels

- aucune capture runtime v4 ou mesure de cadence qualifiée n'existe encore ;
- les hashes de lignée ne deviennent vérifiables qu'avec les sidecars raw et
  census ;
- l'identité NTSC-U/J autorise une projection diagnostique vers PAL, pas une
  équivalence sémantique interrégion.

Aucun octet retail ou C++ généré n'est ajouté.

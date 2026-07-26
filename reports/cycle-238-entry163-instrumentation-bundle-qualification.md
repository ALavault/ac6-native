# AC6 cycle 238 — qualification du bundle d'instrumentation entry-163

Date: 2026-07-18

## Question

Le bundle `ac6-entry163-instrumentation-evidence-v1.zip` contient-il une
nouvelle preuve de jointure MATE→permutation au draw, ou seulement un contrat
permettant de capturer et de refuser proprement les correspondances ambiguës ?

## Identité et intégrité

- archive SHA-256 :
  `2b4d94219731ac6b148bf322b8874e358d45e654ca27f337673c2a13e4a9dba8` ;
- XEX attendu et local :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- `DATA.TBL` attendu et local :
  `82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5` ;
- `DATA00.PAC` local :
  `c3ed20ec6ef0260671d9cd5f3e088fab2a8d983cb6739efab350c87c6fb74816` ;
- les 39 membres couverts par `SHA256SUMS` passent ;
- les huit cas du joint de référence passent : un match exact, six rejets
  ciblés et un rejet de cardinalité double.

Le bundle ne contient pas de binaire retail. Les fixtures de draw et de
catalogue sont synthétiques.

## Recoupement avec le dépôt

Le bundle déclarait trois entrées manquantes. Deux sont déjà fermées dans le
dépôt :

1. `DATA00.PAC` est présent et le vérificateur du bundle accepte l'identité du
   corpus local ;
2. l'outillage natif indexe `DataTable::entries` en zéro-based. Le `entry 163`
   des cycles 234–237 est donc le record à `file+0xA38`, SHA-256
   `321ca1d2bf25832fd28a75be0ae2b876ba5bfe8162f3bb7c1fdfa366968f5627`,
   et non le record one-based 163 à l'index 162.

La troisième entrée reste ouverte : aucun `DrawEvent` retail ordonné ne relie
encore un `MaterialBind` qualifié à la paire VS/PS exacte et à son état. Le
cache persistant qualifié au cycle 237 prouve que 18 hashes de permutations
spéculaires ont été rencontrés, mais il ne conserve ni l'ordre des draws ni le
MATE actif.

## Décision d'intégration

Le contrat utile est conservé comme spécification de la prochaine capture :

- événements `MaterialBind`, `ShaderDef`, `StateDef` et `DrawEvent` ;
- identité par XEX, record `DATA.TBL`, préimage de hash, record MATE, paire
  Xenos exacte et signature d'état ;
- cardinalité exacte à un ;
- rejet stable de toute collision, alias de matériau ou différence d'état ;
- aucune activation d'une équation renderer avant jointure complète.

Le code de référence n'est pas copié tel quel dans le runtime : il ne capture
aucun draw réel et créerait un second chemin de catalogue à côté des parseurs
Xenos déjà qualifiés. La prochaine implémentation doit brancher ce schéma sur
le point de soumission existant, puis alimenter le catalogue natif actuel.

## Statut

- identité du corpus : **confirmed** ;
- base d'index de l'entrée 163 : **confirmed, zero-based** ;
- contrat de rejet synthétique : **confirmed** ;
- capture draw ordonnée : **absente** ;
- jointure MATE→permutation : **needs-dynamic-evidence** ;
- règles renderer activées : **0**.

Aucune session Xenia, VNC ou action humaine n'a été utilisée.

## Commandes exécutées

```text
unzip -t ac6-entry163-instrumentation-evidence-v1.zip
sha256sum -c SHA256SUMS
python3 tests/run_tests.py
python3 tools/verify_inputs.py \
  --assets-dir workspaces/ace-combat-6/game-files
sha256sum workspaces/ace-combat-6/game-files/DATA00.PAC
```

Le vérificateur local termine avec `identity_ok=true`, `join_ready=false` et
les deux seuls codes restants
`AC6_JOIN_E_ENTRY_INDEX_BASIS_UNSET` et
`AC6_JOIN_E_MATE_CATALOG_MISSING`. Le premier est résolu par le contrat
zéro-based du code natif ; le second est la frontière réelle.

# AC6 retail NTSC-U/J — r475 — pas de « plateau » natif : les 86 tirages du sondage tombent TOUS sur le trou de couverture du registre déjà connu (9/9 rejets, « no pinned variant matches this draw state ») — corrige la piste de r473/r474

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r474 : investiguer précisément ce que représentent les 86
tirages du sondage natif et pourquoi rien ne se produit après —
trace/instrumentation du chargement natif, pas une nouvelle tentative
d'entrée.

## Établi — il n'y a PAS de plateau silencieux ; les tirages continuent tant que la fenêtre dure

Relancé (`AC6_NATIVE_VD_TRACE=1 AC6_NATIVE_CAPTURE=1
AC6_NATIVE_PROBE_WINDOW_MS=30000`, ISO réelle). Le journal montre
**36 tirages AVANT** la capture de diagnostic (`capture pixels=...`,
ligne 188) et **50 tirages DE PLUS APRÈS** (lignes 190-244, un seul
lot de vidange `draw=50`), portant le total à **86** — le même chiffre
que r453/r473/r474, confirmant la reproductibilité, mais révélant que
ce chiffre agrège DEUX salves séparées par la capture, pas un compte
figé après un arrêt. Le second lot inclut même le déclenchement du
chemin MSAA de r459 (`r459: RB_SURFACE_INFO=0x0a020280 msaa_bits=2
sample_count=4`) — une preuve directe que le moteur GPU/VD continue de
traiter de nouvelles commandes réelles bien après le point où r473/r474
avaient conclu à un « plateau silencieux ». **Ceci corrige r473/r474** :
ce n'est pas un blocage/plateau au sens où rien ne se passerait — la
fenêtre de sondage (fixe, en millisecondes réelles) capture simplement
un instantané à un moment donné, et du contenu réel continue d'arriver
après cet instantané aussi longtemps que le processus tourne.

## Établi — la vraie raison du contenu noir : TOUS les tirages tombent sur le trou de couverture déjà connu

Sur ce même run : **9 rejets sur 12 vidanges acceptées**, et
**chacun des 9** porte exactement le même message :
```
vd drain pinned execute_frame rejected, falling back to structural
validation: pinned draw: active shader is not pinned: no pinned
variant matches this draw state
```
(les 3 vidanges non rejetées sont de pur travail interne, `draw=0`).
**Les 86 tirages sont TOUS `vertex_count=1 index_count=1`** (auto-indexés,
`index_address=0x00000000`), un motif extrêmement spécifique et
constant à travers TOUTE l'exécution — cohérent avec des éléments
d'interface (icônes/glyphes HUD, un point-sprite expansé par le
vertex shader), PAS avec de la géométrie de scène 3D classique.

**Ceci explique intégralement pourquoi l'extension du registre à 320
entrées (r472) n'a eu aucun effet sur le sondage natif** : ces 320
entrées viennent de la route oracle de vol réel (r470), qui capture
des nuanceurs de GAMEPLAY EN VOL — un ensemble de nuanceurs
complètement différent de celui qu'utilise le contenu de tout début
(logo/titre/interface) que le sondage natif atteint naturellement, sans
même avoir besoin de l'entrée synthétique de r474. Le sondage natif
n'est donc PAS bloqué par une confirmation manquante, ni par un
chargement figé — il tombe simplement, systématiquement, sur le MÊME
trou de couverture de registre déjà caractérisé depuis r456/r462, mais
pour un ensemble de nuanceurs différent (précoce/UI) de celui que la
campagne oracle a exploré jusqu'ici (tardif/vol).

## Non établi

- **La nature exacte du contenu** (logo Namco/Bandai, écran-titre,
  interface de chargement) — non identifiée précisément, seule la
  forme des tirages (1 sommet auto-indexé) et l'absence de
  correspondance dans le registre sont établies.
- **Si étendre le registre avec une capture oracle CIBLÉE sur cette
  phase précoce (au lieu de la phase de vol) résoudrait le contenu
  visible natif** — plausible d'après cette preuve, non testé (une
  nouvelle campagne oracle, hors périmètre borné de ce cycle).
- **Le rôle exact de `fetch_const[1]=0x10000056`/`0x1000001a`** (motif
  répété à travers les tirages, cohérent avec une texture liée) — non
  décodé.

## Décisions prises

- Ne PAS lancer de nouvelle campagne oracle ciblée sur le contenu
  précoce ce cycle — c'est un travail réel et substantiel (répéter le
  cycle complet r463-r472 mais pour une route de démarrage au lieu
  d'une route de vol), hors périmètre d'un cycle d'investigation borné.
- Ne PAS modifier le mécanisme de capture/fenêtre de sondage
  (`AC6_NATIVE_PROBE_WINDOW_MS`) pour tenter de capturer après la
  seconde salve — la preuve établie suffit déjà à expliquer l'absence
  de contenu visible sans qu'un ajustement de fenêtre soit nécessaire
  (le contenu de la seconde salve est du MÊME type de tirage rejeté,
  donc capturer plus tard ne changerait rien).
- Le mécanisme d'entrée de r474 (`AC6_NATIVE_INPUT_AUTO_CONFIRM`) reste
  disponible et committé, mais s'avère non pertinent pour ce blocage
  précis (aucune confirmation n'était en cause).

## Gate

Aucune source de production modifiée ce cycle (analyse par
lancement/trace uniquement, aucun code touché).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
`ctest` racine et natif inchangés (aucune source de production
modifiée ce cycle).

## Named for r476

Une campagne oracle ciblée sur le contenu de tout début (logo/écran-titre,
pas la route de vol de r470) capturerait probablement les nuanceurs que
le sondage natif atteint réellement — c'est le chemin le plus direct
vers du contenu visible natif, mais c'est un travail substantiel
(refaire le cycle r463-r472 pour une route différente), pas une
décision à prendre seul étant donné le budget oracle déjà dépensé cette
campagne. Reste ouvert sinon : décodage de `fetch_const[1]` pour
confirmer l'hypothèse « élément d'interface texturé » ; décision de
committage groupé de l'arriéré natif (r433/r434/r438/r454-r474).

## Files

Aucun artefact gitignoré nouveau conservé (journal de sondage sous
`/fastdata/lavaulta/tmp/`, non conservé).

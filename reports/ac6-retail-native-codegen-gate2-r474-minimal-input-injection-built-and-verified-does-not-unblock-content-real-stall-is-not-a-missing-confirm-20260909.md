# AC6 retail NTSC-U/J — r474 — mécanisme d'injection d'entrée minimal CONSTRUIT et vérifié dans le produit natif, mais n'a AUCUN effet sur le contenu sondé : le blocage natif n'est probablement PAS une simple confirmation manquante, contrairement au chemin oracle

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r473, piste 2 : si un mécanisme d'entrée s'avère
nécessaire, dimensionner sa construction avant de l'entreprendre.

## Établi — le point d'injection existe déjà, réutilisé, pas réinventé

`NativeGuestInputService::get_state()`/`is_connected()`
(`native/src/native_guest_input.cpp`) sont le SEUL chemin par lequel
le code invité recompilé lit un état de manette (via le point
d'import `XamInputGetState`, `tools/materialize_native_import_stubs.py`).
En environnement bac à sable sans manette physique, `SDL_IsGameController`
renvoie toujours faux — le jeu invité voit « aucune manette » à
chaque sondage, en continu, depuis le boot.

## Correctif appliqué et vérifié (local, committé — fichier propre à HEAD, PAS l'arriéré entremêlé)

Un contournement minimal, gardé par variable d'environnement
(`AC6_NATIVE_INPUT_AUTO_CONFIRM`), ajouté directement dans
`get_state()`/`is_connected()` pour l'utilisateur 0 : simule une
manette connectée qui pulse le bouton A selon un cycle fixe (15 ticks
pressé / 15 relâché, ~0,5 s à 60 Hz) — sans connaître le tick exact
requis, cette pulsation garantit qu'une éventuelle boîte de dialogue
de confirmation (l'équivalent natif du « Deploy with this selection? »
que r470 a dû corriger côté oracle) reçoit tôt ou tard un appui A.
Comportement par défaut (variable absente) **totalement inchangé** —
zéro risque pour le chemin déjà vérifié. Contrairement aux fichiers
`native_vulkan_backend.{h,cpp}` etc., `native_guest_input.cpp` était
**propre à HEAD** avant ce cycle (dernier commit `efa53e83`, r180) —
ce correctif est une addition isolée, pas un morceau de l'arriéré
entremêlé, et est committé directement (voir Gate).

## Établi — vérifié sans régression, MAIS sans effet sur le contenu

`ctest` natif complet : **11/11**. Relancé avec
`AC6_NATIVE_INPUT_AUTO_CONFIRM=1` ET une fenêtre de sonde portée à
**90 s** (3,6× la limite précédente de 25 s, toujours bien en-deçà des
573 s du chemin oracle, mais dans la plage recommandée pour ce cycle) :
```
86 vd draw (STRICTEMENT IDENTIQUE à r453/r473, 25 s)
presented_frames=5 state=2
capture pixels=921600 non_black=0 distinct_colors=1
```
**Aucun changement, ni en nombre de tirages ni en contenu**, malgré
l'entrée synthétique ET une fenêtre 3,6× plus longue. Le journal
confirme que les 86 tirages et les événements associés se produisent
tous tôt dans l'exécution, puis plus rien de nouveau n'apparaît
jusqu'à la fin de la fenêtre — ce n'est pas un écran qui continue de
se rafraîchir en attendant une confirmation (comme le chemin oracle,
qui présentait en continu pendant 573 s), c'est un **plateau
silencieux** après les 86 tirages initiaux.

## Ce que ceci établit (correction de trajectoire pour r475)

**L'hypothèse de travail de r473 — « comme le chemin oracle, il
manque juste une confirmation » — est affaiblie par cette preuve.**
Le chemin oracle continuait à rendre/présenter activement pendant
573 s en attendant la confirmation (r469) ; le chemin natif ne
produit RIEN de nouveau après les 86 tirages initiaux, avec ou sans
entrée simulée, sur une fenêtre 3,6× plus longue. Ceci ressemble
davantage à un blocage de contenu/chargement propre au produit natif
(distinct du blocage de confirmation du produit oracle-hybride) qu'à
un problème d'entrée manquante.

## Non établi

- **La nature exacte du plateau après 86 tirages** — non
  investigué ce cycle (nécessiterait une trace plus fine du
  chargement/threading natif, hors périmètre borné de ce cycle).
- **Si une fenêtre encore plus longue (573 s) sans rapport avec
  l'entrée changerait quoi que ce soit** — non testé, la preuve du
  plateau silencieux rend cet essai peu prometteur sans comprendre
  d'abord sa cause.

## Décisions prises

- Construire le mécanisme minimal (portée respectée : point
  d'injection déjà existant réutilisé, pas de nouveau sous-système)
  et le committer directement — fichier propre à HEAD, addition
  isolée et vérifiée, pas l'arriéré entremêlé.
- **Ne PAS pousser plus loin ce cycle** (essayer d'autres boutons,
  fenêtres plus longues, ou investiguer le plateau) — la preuve
  obtenue affaiblit l'hypothèse de départ plutôt que de la confirmer ;
  continuer sans nouvelle piste serait un forage à l'aveugle.

## Gate

```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
`ctest` racine : inchangé pour le reste du dépôt ; `ctest` natif
reconfirmé 11/11 avec le correctif appliqué.

## Named for r475

Investiguer précisément ce que représentent les 86 tirages et
pourquoi rien ne se produit après (trace/instrumentation du
chargement natif, pas une nouvelle tentative d'entrée) — le
mécanisme d'injection construit ce cycle reste disponible et
réutilisable (`AC6_NATIVE_INPUT_AUTO_CONFIRM`) une fois la vraie
cause du plateau comprise. Le registre à 320 entrées (r472) reste
correctement installé ; cette découverte n'invalide rien de son
travail.

## Files

Aucun artefact gitignoré nouveau conservé (journal de sonde sous
`/fastdata/lavaulta/tmp/`, non conservé).

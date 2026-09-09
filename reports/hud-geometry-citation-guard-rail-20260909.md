# Garde-fou HUD — règle CLAUDE.md + vérificateur `audit_hud_geometry_citations.py` (2026-09-09)

Piste C du plan `groovy-beaming-book.md`, approuvée par l'utilisateur,
indépendante des Pistes A/B (`recompilation/ace-combat-6-retail`).

## Établi

- `reconstruction/ace-combat-6/{include/ac6,src}/native_hud_gpu_overlay.{h,cpp}`
  sont toujours non suivis par git. `reports/ac6-native-visual-shader-hud-20260829.md`
  les documente comme un outil de diagnostic explicitement non promu à la
  parité retail — l'overlay dessine des rectangles à coordonnées pixel
  codées en dur (`add_outline_px(0U, 16.0F, 608.0F, 316.0F, 704.0F, 2.0F)`,
  etc.) pour garder le HUD visible dans un chemin où le gate `mission01_unqualified`
  refuse encore la présentation GPU retail. Cette garde vivait uniquement dans
  le rapport, jamais dans le code ni dans `CLAUDE.md`.
- Fait notable trouvé pendant l'investigation : la copie de travail non
  committée de `reconstruction/ace-combat-6/CMakeLists.txt` référence déjà
  `src/native_hud_gpu_overlay.cpp` comme source de build, mais **la version
  committée (`HEAD`) ne la référence pas du tout** — le fichier `.cpp` est
  entièrement absent du build à `HEAD`. Il n'y a donc pas de rupture de build
  latente pour un clone frais aujourd'hui ; le lien build↔fichier n'existe
  que dans l'arriéré massif déjà non committé de `reconstruction/ace-combat-6`
  (~240 fichiers, hors périmètre de ce cycle), pas dans l'état committé.

## Décisions prises

- **Règle écrite dans `CLAUDE.md`** (nouvelle section « HUD pixels need a
  retail draw, not a hand-picked rectangle ») : tout pixel HUD dans un
  chemin de livraison gaté doit provenir d'un tirage retail réellement
  exécuté via la chaîne épinglée shader/texture
  (`recompilation/ace-combat-6-retail`, r256-r259 et suivants, vérifié comme
  la bonne référence) ; rien sous `reconstruction/` ne compte comme preuve
  HUD retail.
- **Marquage du code existant, pas déplacement** : un commentaire d'en-tête
  a été ajouté aux DEUX fichiers (`.h` et `.cpp`), citant
  `reports/ac6-native-visual-shader-hud-20260829.md` et déclarant
  explicitement « diagnostic-only ... must not be used as a source of HUD
  pixel data in a gated delivery path ». Déplacer les fichiers a été écarté :
  cela aurait nécessité de toucher `CMakeLists.txt`, entremêlé avec
  l'arriéré massif non committé de `reconstruction/ace-combat-6`, hors
  périmètre de ce cycle.
- **Vérificateur `tools/audit_hud_geometry_citations.py`**, même patron que
  les audits `tools/audit_*.py` existants (docstring expliquant le
  raisonnement + un incident réel, code 0/1/77). Détecte un appel nommé
  rect/outline/panel/hud/overlay/marker/reticle/radar portant 4+ littéraux
  flottants `N.NF` consécutifs (la forme `add_outline_px`) sans citation
  `reports/*.md` dans le fichier. Portée par défaut : `recompilation/`
  (le chemin gaté) ; `reconstruction` peut être passé en argument pour
  vérifier aussi l'arbre de diagnostic. Exclut `thirdparty/`/`build/`/
  `upstream/` (code tiers vectorisé, non-auteur de ce dépôt — la première
  version, sans cette exclusion, remontait 4 faux positifs dans `imgui.cpp`
  vendu deux fois).
- **Fichiers HUD laissés non suivis** : maintenant qu'ils portent la
  citation, les commettre nécessiterait aussi de commettre le lien
  `CMakeLists.txt` qui les active dans le build — entremêlé avec l'arriéré
  massif non lié de `reconstruction/ace-combat-6`. Les deux options étaient
  défendables selon le plan approuvé ; celle-ci évite d'élargir le périmètre
  de ce cycle.

## Vérifié

- `python3 tools/audit_hud_geometry_citations.py reconstruction` : **échoue**
  avant la citation (1 violation, `native_hud_gpu_overlay.cpp`), **passe**
  après (`violations=0`) — comportement avant/après exactement comme demandé
  par le plan.
- `python3 tools/audit_hud_geometry_citations.py` (portée par défaut,
  `recompilation/`) : passe, `scanned=301 violations=0`.
- Les trois audits de contrats
  (`audit_ac6_mission01_native_gate.py`, `audit_ac6_contract_artifacts.py`,
  `audit_ac6_contract_addresses.py`) : tous verts, inchangés.
- `ctest` racine (`reconstruction/ace-combat-6/build`) : seul l'échec
  préexistant `ac6-cpp-complexity`, sans rapport avec ce cycle.
- `python3 tools/audit_claude_md_numbers.py CLAUDE.md` : deux mismatchs
  préexistants (`0x822A23D8`, `0x82263A50`), sans rapport avec l'ajout —
  aucun chiffre cité dans la nouvelle section.

## Non établi

- Si l'arriéré massif de `reconstruction/ace-combat-6` (dont
  `CMakeLists.txt`) sera un jour committé, et si les fichiers HUD seront
  alors inclus — décision distincte, hors périmètre de ce cycle.

## Files

Committé : `CLAUDE.md`, `tools/audit_hud_geometry_citations.py`, ce rapport.
Édités mais non committés (fichiers déjà non suivis, arriéré non lié) :
`reconstruction/ace-combat-6/include/ac6/native_hud_gpu_overlay.h`,
`reconstruction/ace-combat-6/src/native_hud_gpu_overlay.cpp`.

# AC6 retail NTSC-U/J — r466 — le « movie worker » nommé par r465/r466 est un VRAI thread invité (pas un artefact hôte) déjà instrumenté depuis le tout premier commit du sous-module vendu ; il ne bloque jamais, il tourne en boucle serrée sans jamais attendre réellement — cause racine non établie, prochaine étape concrète nommée

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucune tentative oracle ce cycle — investigation par lecture de code et
de journal, sans relance de route (données déjà capturées par r465
réutilisées).

## Contexte

Nommé par r465/r466 : la piste « étendre le registre épinglé via
l'oracle » (r454-r465) bute sur un blocage du « movie worker » en
cinématique, déjà documenté deux fois (r465 lui-même, et
`reports/retail-us-mission01-flight-long-candidate-20260828.md`) mais
jamais tracé jusqu'à sa cause. Ce cycle reprend le journal déjà
capturé par r465
(`/fastdata/lavaulta/tmp/ac6-oracle-r465-shader-capture/ac6recomp.log`,
30 233 080 octets, toujours présent) plutôt que de relancer une route
oracle coûteuse.

## Établi — le « movie worker » est un VRAI thread invité, pas un artefact hôte

`grep -rln "movie worker"` dans le sous-module localise l'unique
source :
`thirdparty/rexglue-sdk/src/kernel/xboxkrnl/xboxkrnl_threading.cpp`,
lignes 102-104 :
```c++
constexpr uint32_t kAc6MovieWorkerSetEvent = 0x82916E3C;
constexpr uint32_t kAc6MovieWorkerWaitEvent0 = 0x82916E2C;
constexpr uint32_t kAc6MovieWorkerWaitEvent1 = 0x82916E08;
```
Ces trois adresses (`0x829xxxxx`) sont des adresses **invitées** (tas
du jeu), pas des pointeurs hôtes — le « movie worker » est un vrai
thread créé par le jeu lui-même (`XThread`), pas un shim de
compatibilité côté hôte. `xeKeSetEvent`/`KeWaitForMultipleObjects`
reconnaissent ces trois adresses spécifiques et journalisent chaque
appel avec le préfixe `AC6 movie worker`.

**Cette instrumentation PRÉCÈDE toute cette campagne** :
`git log -p -S "kAc6MovieWorkerSetEvent"` sur ce fichier dans le
sous-module remonte au commit `ddf7c285` (« Initial barebones
version », auteur `salh`, 17 avril 2026) — le tout premier commit du
sous-module vendu `AC6_recomp`. Les auteurs d'origine de ReXGlue/
AC6_recomp avaient donc déjà identifié et nommé ce thread précis avant
que ce dépôt n'existe. Aucun rapport de ce dépôt (`grep -rl` sur les
trois adresses dans `reports/*.md`) ne les cite — c'est la première
fois qu'ils sont tracés jusqu'à leur source ici.

## Établi — le thread ne bloque JAMAIS : boucle serrée, pas une attente réelle

Sur le journal de 573 s capturé par r465 : **159 644 lignes** portant
`AC6 movie worker` (environ 80 000 paires SetEvent/
KeWaitForMultipleObjects). L'intervalle typique entre deux lignes
consécutives est de l'ordre de la milliseconde ou moins (ex.
`23:55:45.373` → `23:55:45.374`), et **chaque** `KeWaitForMultipleObjects`
journalisé porte `result=0` (`WAIT_OBJECT_0` — le PREMIER objet était
déjà signalé, retour immédiat, jamais de blocage réel). Ce n'est pas un
thread endormi qui ne se réveille jamais (un « hang » classique) : **c'est
une boucle qui ne bloque jamais**, consommant du temps CPU en continu
sans jamais réellement attendre — cohérent avec au moins un des deux
objets (`0x82916E2C` ou `0x82916E08`) laissé signalé en permanence par
un bug (état non réarmé) plutôt qu'avec une charge de travail réelle et
progressive.

## Établi — corrélation avec la cinématique réelle : une vraie cutscene se termine, mais le monde ne démarre jamais après

`render_hooks.cpp:372-378` (`ac6CinematicTickHook`) : le drapeau
`cinematic` est piloté par un « demo-manager Exec »
(`sub_82184460`/`sub_821856F8`, code PPC recompilé réel du jeu) qui
tamponne `g_last_cinematic_tick_ms` À CHAQUE frame tant qu'une
cutscene EN MOTEUR (pas une vidéo FMV — aucune trace de décodage
XMV/WMV nulle part dans ce sous-module, `grep -rln "XMV\|VideoDecode"`
ne trouve rien) est en cours ; `IsCinematicActive()` (ligne 185-194)
décroît ce drapeau après 100 ms d'inactivité de ce tampon.

Séquence observée dans `[ac6-visual-phase]` (`render_hooks.cpp:311-316`) :
```
23:55:44.149  cinematic=0 world=0 hud=0 stable=0   (démarrage)
23:55:45.754  cinematic=0 world=0 hud=1 stable=0   (menus)
23:57:35.898  cinematic=1 world=0 hud=1 stable=0   (une VRAIE cutscene démarre)
23:58:22.182  cinematic=0 world=0 hud=1 stable=0   (~46 s plus tard : la cutscene se termine)
                                                     — PLUS AUCUNE ligne jusqu'à la fin du run
                                                       (00:05:07, soit 6 min 45 s plus tard)
```
La cutscene démarre bien, tourne ~46 s (une durée plausible pour un
briefing de mission), puis se termine (le hook cesse d'être appelé,
cohérent avec une fin réelle du code PPC de cutscene, pas un crash
silencieux — `sub_82184460`/`sub_821856F8` ARRÊTENT d'être exécutés,
ce qui est le comportement attendu en fin de cutscene). **Mais le
monde (`world`) ne devient JAMAIS actif** pour le reste du run : le
jeu reste bloqué en HUD seul (menus), sans jamais atteindre le rendu
3D de vol, alors que le thread « movie worker » continue sa boucle
serrée sans interruption pendant tout ce temps.

## Ce que ceci établit, et ce qui reste ouvert

**Établi** : le blocage n'est PAS un crash, PAS un thread endormi qui
ne se réveille jamais, PAS un problème du hook de présentation/
cinématique lui-même (qui se comporte de façon cohérente : il détecte
correctement le début et la fin de la cutscene). C'est une boucle
d'attente réelle côté invité (le « movie worker ») qui ne bloque
jamais correctement, dont l'origine précède cette campagne (code
vendu, jamais modifié par ce dépôt), et dont la persistance après la
fin de la cutscene coïncide exactement avec l'absence de transition
vers le monde 3D.

**Non établi (cause racine précise)** : LEQUEL des deux objets
(`0x82916E2C` ou `0x82916E08`) reste signalé en permanence, ni QUEL
autre sous-système (ou thread) est censé le réarmer/consommer, ni si
le jeu attend une confirmation de ce thread avant d'autoriser la
transition vers le monde. Une session gdb en direct — point d'arrêt
sur le retour de `KeWaitForMultipleObjects` au moment précis où
`result=0`, lecture de l'état interne des deux `XEvent`/`XSemaphore`
correspondants (signalé/non signalé, type auto-reset vs manuel) — est
nécessaire pour trancher entre (a) un stub hôte incomplet (une
fonctionnalité que la vraie console fournirait mais que ce hôte ne
fournit pas ou fournit de façon incorrecte) et (b) une limitation déjà
connue des auteurs d'origine de ReXGlue/AC6_recomp (cohérent avec le
fait que cette instrumentation existe depuis leur tout premier
commit — ils suivaient déjà ce thread précis).

## Décisions prises

- Ne pas relancer de route oracle ce cycle (le journal déjà capturé
  par r465 suffisait pour cette investigation par lecture de code) —
  économise le budget oracle pour une tentative future mieux ciblée.
- Ne pas tenter de correctif spéculatif sur `xboxkrnl_threading.cpp`
  sans avoir d'abord identifié, par preuve en direct (gdb), lequel des
  deux objets reste signalé et pourquoi — un correctif à l'aveugle sur
  du code tiers vendu, sans preuve, serait exactement le type de
  « règle plausible sans contrôle » que ce dépôt refuse.
- Aucun état de build touché ce cycle (investigation purement par
  lecture de code source et de journal déjà capturé) — aucune
  restauration du profil `native` nécessaire.

## Gate

Aucune source ni état de build modifié ce cycle. Gates de dépôt
exécutés pour clôturer le cycle normalement :
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
`ctest` racine inchangé (seul l'échec préexistant connu
`ac6-cpp-complexity`).

## Named for r467

**Prochaine étape concrète** : une session gdb en direct sur le
binaire oracle (`build/ntsc-uj/cmake/ac6recomp`, profil
`rexglue-oracle`, déjà buildable depuis r465) — point d'arrêt sur le
retour de `xeKeSetEvent`/`KeWaitForMultipleObjects` pour les trois
adresses `0x82916E3C`/`0x82916E2C`/`0x82916E08`, lecture de l'état
interne des objets noyau correspondants au moment précis où le run se
bloque (~23:58:22, juste après la fin de la cutscene captée par r465).
Ceci tranchera entre stub hôte incomplet et limitation déjà connue des
auteurs d'origine. Une fois la cause identifiée, la piste « étendre le
registre épinglé via l'oracle » (r454-r465) pourra reprendre. Reste
ouvert, non bloquant : (1) une fois le câblage `PinnedShaderRuntime`
jugé mûr, reconsidérer le committage groupé de l'arriéré natif
(r433/r434/r438/r454-r462) ; (2) mise à jour de
`tools/prepare.py`/`tools/build.py` pour le chemin ISO par défaut du
profil natif (confort, pas une nécessité).

## Files

Aucun artefact gitignoré nouveau (journal réutilisé depuis
`/fastdata/lavaulta/tmp/ac6-oracle-r465-shader-capture/`, non
committé, déjà présent avant ce cycle).

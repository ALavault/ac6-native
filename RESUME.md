# Reprise cycle 1824 — START tombe sur un splash figé

L'hypothèse utilisateur est **confirmée** : au moment où START est envoyé,
l'écran montre le splash éditeur Bandai Namco Games, pas un titre interactif.
Readback byte-identique à celui du tick 1160 ; Title immobile à l'état interne 1
du tick 2452 au tick 8000.

**Ne pas chercher pourquoi la timeline SWG serait gelée : elle ne l'est pas.**
C'est une conclusion de ce cycle qui a été mesurée puis réfutée dans le même
cycle. L'owner racine `0x2E3CDD10` (2220 frames) avance d'une frame toutes les
trois ticks depuis le tick 225 ; les owners à 1 et 2 frames sont immobiles par
conception. Le film se termine vers le tick 6882, et START au tick 3000 tombe à
42 % de sa durée.

Reprendre par le résultat du run
`artifacts/goal-playable/start-after-brand-movie-20260824/` : START au tick 7200
(après la fin du film), borne 9000, backend headless. S'il produit une
transition, refaire un run rendu pour la capture validée et inspectée. Sinon,
chercher ce que la dernière frame du film produit et pourquoi le titre n'est pas
armé.

Ne pas toucher au renderer ni à la couleur (décision utilisateur :
« functional-enough »). Ne pas rouvrir « qui arme `manager+0x18` » avant d'avoir
le verdict de ce run.

Pièges déjà payés : boutons `--input-at` en **décimal** (START = 16, `0x0010`
est rejeté) ; ne pas attendre sur un `runtime.status` résiduel ;
`AC6_DEMO_AUDIT_SCREENCAP_DIR` est fail-closed et trappe au tick 182 ; le
watcher draw-selector plafonne à 512 événements (~tick 480).

Outils de ce cycle : `tools/analyze_title_frame_timeline.py`
(`timeline` / `describe` / `diff`, instrument calibré au pixel près) et
`artifacts/goal-playable/title-start-timing-capture-20260824/run-timeline-sampled.sh`
(échantillonneur : copier, valider 2 764 816 octets, hacher la copie, puis
nommer — le runtime écrit non atomiquement).

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Reprise cycle 1823 — pivot gameplay, captures obligatoires

**Redirect utilisateur (2026-08-24)** : viser le début du gameplay ; produire
des captures pour validation automatique **et** humaine à chaque run.

Ne pas reprendre le fil couleur `BF` du logo : il est **parké** (ni réfuté ni
supersédé), frontière conservée aux callsites `0x82322438`/`0x82324118`. Les
contrats mission01 posent `visual_parity_out_of_scope: true`.

Hygiène : `ctest` 26/26, tous les audits verts, arbre commité. La baseline de
complexité est re-pinnée (cliquet intact, budgets globaux inchangés).

Reprendre par le résultat du run
`artifacts/goal-playable/title-start-timing-capture-20260824/` : run naturel
sans entrée, 8000 ticks, une capture par présentation. Établir la chronologie
des frames distinctes, puis décider si START doit être envoyé plus tard que le
tick 3000. Gates suivants déjà nommés : writers de `Title+0x48/+0x49`; les deux
gardes restantes de `sub_8217E258` (`[0x2E3C0200+12]==1`, `[0x2E3C0200+136]==0`)
avec la réponse du handler, watchées jusqu'à 8000 ticks ; handler de l'item
`+0x1788`.

Ne pas repartir sur le flag de readiness `[*0x827435F8+0x222BFE]` : **réfuté
comme blocage** (pool-poison `0xFE` au tick 4, garde `!= 0` déjà satisfaite, le
forcer à 1 ne change rien).

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Reprise cycle 1822 — alpha ramené au transform parent

`0x82323BB8` reçoit le transform en `r4/r30`. Pour le type 4, que le record
`controller+0x08/+0x04` existe ou non, il écrit exactement
`C14c=transform[0x4c]` et `C15c=transform[0x5c]`. L'entrée alpha du pack P1
est donc `transform[0x4c]+transform[0x5c]`; le prédicat record présent/absent
est réfuté comme cause de `BF`.

Reprendre statiquement aux callsites externes `0x82322438` et `0x82324118` :
qualifier celui du contrôleur titre puis le builder des cinq composantes
`r4+0x40/+0x44/+0x48/+0x4c/+0x5c`. Aucun runtime ni changement renderer.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Reprise cycle 1821 — type 4 réduit à la constante identité

`0x8264CDF8[4]=0x82322EC8`, et ce handler retourne toujours le global
`0x8264CDA0 = [1,1,1,1,0,0,0,0]` en floats. Ainsi `T+0x0c=1`, `T+0x1c=0`
et l'octet `BF` dépend uniquement de
`clamp(MovieController[0x14c]+MovieController[0x15c],0,1)`. Reprendre dans
`0x82323CF4..0x82323D60` sur les stores `+0x14c/+0x15c` et leur prédicat.
Aucun runtime ni correctif renderer/readback.

# Reprise cycle 1820 — matrice couleur jointe

`r5` de `0x820EB200` est maintenant joint à `M=r1+0x50` dans `0x82326420`.
Le chemin exact est `0x82326540 r6=M -> 0x82325E70 -> 0x82325EB8 r5=M ->
slot 7`. Les cinq champs couleur sont produits par les stores
`0x823264A4/C8/EC`, `0x82326510` et `0x82326524` depuis le pointeur `T` rendu
par `0x823237B8` et les coefficients `MovieController+0x140..+0x15c`.
Reprendre uniquement à l'entrée type 4 `0x8264CDF8[4]`. Ne pas lancer de
runtime ni modifier renderer/readback avant d'avoir résolu cette cible.

# Reprise cycle 1819 — pack couleur du record fermé

Le gate `record+0x18` est fermé. `0x821DEED8` ne copie pas un champ unique : il
assemble les octets quantifiés de `parameters+0x3c/+0x30/+0x34/+0x38`. Le bit
de garde `parameters+0x40 & 0x20` est toujours actif sur le chemin titre car
`0x820EB200` ajoute `0x860`. Les sources sont `r5+0x40/+0x44/+0x48` et
`clamp(r5+0x4c+r5+0x5c,0,1)`. `BFFF0000` implique `[BF,FF,00,00]` après
quantification. Reprendre uniquement au producteur/type de ce `r5`; ne pas
modifier renderer/readback et ne pas lancer de runtime avant épuisement des
xrefs/stores statiques.

# Reprise cycle 1818 — constructeur du record P1 fermé

Ne plus poursuivre `0x821185A8` comme producteur amont : son store `+0x18`
copie `source+0x24` vers un owner de travail construit après le parcours.

Le nœud source est réservé par `0x82095DF0`, initialisé par `0x821DEED8`, puis
publié par `owner+0x20`/`owner+0x24` et `tail+0x10`. Au callsite titre,
`0x820EB200` passe l'owner `0x8281EAF0` et son bloc local `r1+0x50`.

Reprendre uniquement dans `0x821DEED8` : trouver les stores à `r31+0x18`,
remonter leur valeur et leur condition à `r30`, puis joindre ce champ au store
du bloc `r1+0x50` dans `0x820EB200`. Aucun runtime ni changement renderer.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Reprise cycle 1817 — record P1 identifié comme nœud de liste

La chaîne est fermée jusqu'au champ source : `0x82119488` parcourt les owners
de `r31`, prend `record=*(owner+0x20)`, avance par `record+0x10`, puis appelle
`0x82118650` avec `r5=record`. Celui-ci sélectionne le writer par
`0x82009E78 + *(record+0x14)*0x14 + 0x0C` et lui transmet `r4=record`.
`0x82118D18` lit donc précisément `record+0x18`.

Le prochain pivot est `0x821185A8`, qui contient
`0x8211860C: stfs fr12,0x18(r31)`. Ne pas le promouvoir : ni l'identité du
`r31` avec le nœud de liste, ni les bits de `fr12` ne sont prouvés. Reprendre
par les appelants, l'allocation/retour et l'insertion dans `owner+0x20` ou
`record+0x10`. Aucun runtime ni correctif renderer. `frontend=false`,
`mission=false`, `terminal=false`, `supported=false`.

# Reprise cycle 1816 — table indirecte P1 localisée

Le `basefile` qualifié contient deux entrées de `0x14` pour le writer
`0x82118D18` : `0x82009E8C` et `0x82009EB4`, fonction à `entry+0x0C`.
Discriminants bruts `+0x04=09000001` et `09000003`; champs communs
`+0x00=0D`, `+0x08=826F61C0`, `+0x10=821187A8`. Il n'existe aucune branche
directe ni construction des adresses de cellules exactes.

Reprendre par les xrefs de la base/du voisinage `0x82009E8C`, identifier le
calcul `index*0x14`, le load `+0x0C` et le `mtctr/bctrl`. Suivre ensuite `r4`
jusqu'au store `+0x18`. Aucun watchpoint avant épuisement de cette frontière.

Preuves :
`artifacts/goal-playable/title-p1-dispatch-xref-static-20260824/BLOCKER.md`,
`basefile-qualification.txt`, `qualified-xref-scan.txt`. Aucun runtime ni code
modifié. `frontend=false`, `mission=false`, `terminal=false`,
`supported=false`.

# Reprise après cycle 1814

Le fetch réellement consommé est fermé : slot 95/P1, base `0x103FB890`, mot
guest `BFFF0000`, résultat shader `(191,0,0,255)`. Q1/94 n'est pas remappé et
ne doit plus servir de source à l'analyse du draw.

Reprendre au writer `0x82118D18` : `0x82118D8C` charge `r4+0x18` et les quatre
stores copient directement ce dword aux couleurs P1. Identifier statiquement
le record `r4`, son dispatch et le producteur de `+0x18`; aucun nouveau rendu
avant cette jointure. Voir `NEXT.md` et
`artifacts/goal-playable/title-vfetch95-color-semantics-static-20260824/RESULT.md`.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Reprise précédente — cycle 1813

Le rouge est antérieur au readback : le PS fait `texture * couleur vertex` et
écrit directement une cible RGBA8. Ne plus toucher à `copy_dest_swap`, au
swizzle de présentation, au PPM ni à la vue texture arrayée pour corriger ce
fond.

La preuve rouge existante porte cependant sur le slot 94, tandis que `vf0`
consomme le slot 95 dans le bridge. Reprendre uniquement par l'extraction des
deux dwords du slot 95 au draw qualifié, puis appliquer son propre décodage
FMT/endian/swizzle. Voir `NEXT.md` et
`artifacts/goal-playable/renderer-color-parallel-20260824/RESULT.md`.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Reprise précédente — cycle 1812

Le fetch couleur est fermé sans modification du produit. Le slot 94 utilise
le mode 3 `k16in32`, pas `k8in32`; `FF FF 00 00` devient rouge opaque après
échange des demi-mots, unpack RGBA8 et swizzle `zyxw`. Le SPIR-V matérialisé
confirme littéralement ce flux. Ne plus corriger le readback, la présentation
ni le décodeur vertex pour fabriquer le blanc.

Reprendre au producteur guest du dword à `vertex+0x30` dans Q1. Valoriser
d'abord les writers connus `0x82118D18`/`0x82119048` et leur record source. Un
watchpoint dynamique n'est permis que si cette indirection reste la frontière,
borné aux quatre mots couleur et au premier draw. Voir `NEXT.md` et
`artifacts/goal-playable/title-vfetch-color-semantics-static-20260824/RESULT.md`.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Reprise précédente — cycle 1811

Le swizzle de présentation `B,G,R,1` est corrigé et testé dans le build
codegen-on. La capture obligatoire n'est plus bleue : elle est rouge plein
écran, avec « Games » et les trois lobes visibles en rouge sombre. Les coins
valent `(255,0,0)` et seul le canal R est présent; le fond blanc attendu n'est
pas produit en amont.

Reprendre statiquement au premier quad plein écran : ses quatre couleurs
vertex journalisées valent `0xFFFF0000`, celles du logo `0xBFFF0000`, et le
pixel shader les multiplie par la texture. Fermer la sémantique exacte du
fetch `FMT_8_8_8_8` et de son endianness avant toute nouvelle correction ou
capture. Voir `artifacts/goal-playable/title-present-swizzle-20260824/`.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Reprise précédente — cycle 1809

Le témoin ancien est définitivement capturé : 53 tuples `NtReadFile`, 502
soumissions, 407 présentations, mais readback entièrement noir. Ne pas le
relancer.

Le prétendu côté courant était `build/` codegen-off et a refusé le probe à
tick 0. Reprendre par le rebuild cgroup de `build-codegen-on`, puis un seul run
courant avec `AC6_DEMO_WATCH_NT_READ_FILE=1` et la même borne 1160. Voir
`artifacts/goal-playable/ntread-first-divergence-20260824/BLOCKER.md`.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Reprise précédente — cycle 1808

`0x821A61EC` est fermé statiquement : `Function_821A6168` appelle le slot
file-I/O `+0x10` de la table `0x823C2D2C`, traite `STATUS_PENDING` et peut
attendre le handle de complétion. Le bridge courant joint ce chemin à
`NtReadFile`, puis publie immédiatement l'événement après lecture synchrone.

L'ancien binaire contient le même helper auto-reset et l'appelle aussi dans sa
branche `NtReadFile`; la différence locale de contrat est réfutée. Reprendre
par une capture bornée du premier tuple d'entrée `NtReadFile` divergent. La
validation ultérieure devra produire une capture inspectée; noir reste un échec. Voir `NEXT.md` et
`artifacts/goal-playable/event-site-821a6168-static-20260824/RESULT.md`.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Reprise précédente — cycle 1807

La première divergence est au site invité `0x821A61EC` / LR `0x821A61F0` :
le courant y publie seize handles `0xE0000088…0xE00000C8`, absents de
l'ancien, avant de bloquer tous ses threads. L'atlas situe le site dans
`Function_821A6168` mais ne contient pas son pseudocode.

Après cinq batches, reprendre par un export Ghidra strictement ciblé de cette
fonction dans `ghidra-projects/ace-combat-6-demo`; aucun runtime ni Edge avant
cette lecture. Voir `NEXT.md` et
`artifacts/goal-playable/first-guest-divergence-20260824/BLOCKER.md`.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Reprise précédente — cycle 1806

Le binaire codegen-on conservé du 22 août restaure exactement le ring à 1160
ticks : 502 soumissions, 4405 dwords et 407 présentations. Son readback est
pourtant uniformément noir. Le hook `AC6_PPC_STORE_U32(0x7FC80714)` existe
dans l'ancien et le courant; ne plus chercher une suppression du hook.

Reprendre sur la première divergence guest/scheduler en amont, en exploitant
le contraste ancien `409 VdSwap` contre courant `1052 VdSwap` et zéro ring.
Voir `NEXT.md` et
`artifacts/goal-playable/pm4-old-codegen-on-b-1160-20260824/RESULT.md`.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Reprise précédente — cycle 1805

Le binaire courant échoue aussi à la borne historique de 1160 ticks : ring
initialisé mais `RPTR=WPTR=0`, zéro présentation renderer et aucun readback.
La régression native antérieure au publisher `CP_RB_WPTR` est prouvée ; ne plus
chercher START ni un reset tardif.

Chercher d'abord une copie du binaire/objet positif encore conservée, puis
attribuer le premier changement causal postérieur au 23 août 14:50. Voir
`NEXT.md` et
`artifacts/goal-playable/pm4-epoch-1160-current-20260824/RESULT.md`.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Reprise précédente — cycle 1804

Ne plus attribuer l'écran noir à START : un A/B strict donne zéro soumission
PM4 et zéro présentation renderer dans les runs neutre et START. Le contrôle
positif historique a été inspecté et montre le logo Namco non noir.

Reprendre statiquement sur la régression `CP_RB_WPTR` dans le chemin commun
ring/MMIO modifié après le run positif, en priorité `graphics_mmio_cpu.hpp`,
`graphics_ring.hpp` et `guest_bridge.cpp`. Voir `NEXT.md` et
`artifacts/goal-playable/post-start-pm4-ab-20260824/RESULT.md`.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Reprise précédente — cycle 1803

La chaîne naturelle est fermée : Q `0x8270F598` devient payload `node+0x0C`,
le callback `node+0x08` vaut `0x821187A8`, puis appelle `0x821B4D80`.

Reprendre par un oracle Xenia Edge démo borné, jamais retail : comparer le
premier kickoff GPU et la screencap Edge au natif (`WPTR=0`, image noire).
Voir `artifacts/goal-playable/q-to-callback-static-20260824/RESULT.md` et
`NEXT.md`. `supported=false`.

# Reprise précédente — cycle 1802

Le record `0x827B3A80` du draw `0x12` franchit readiness et `record+0x14`,
puis suit `0x82118FA0 -> 0x821185A8(Q=0x8270F598) -> 0x821186B0`. Le worker
`0x821187A8 -> 0x821B4D80` existe, mais ses appels capturés précèdent ce lot et
ne lui sont pas attribuables.

Reprendre statiquement sur `0x82118B88`, la queue `0x822DA568` et le callback
`0x822E35E8`. Le seul artefact manquant est le champ/token Q→P permettant de
reconnaître `0x8270F598` à `0x821187A8`. Voir `NEXT.md` et
`artifacts/goal-playable/title-record-consumer-runtime-20260824/BLOCKER.md`.

Le ring reste vide et 15 captures vérifient une sortie noire.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Reprise précédente — cycle 1801

Les seize draws naturels `list13/index 0x12` sélectionnent le handle valide
`0x0E000071` et atteignent tous la création d'un record de 0x70 octets :
`0x820EB200 -> 0x820EA9A0 -> 0x82095DF0 -> 0x821DEED8`. Ne plus chercher un
rejet dans SWG, l'index de draw ou la table de handles.

Reprendre statiquement au consumer du record :
`0x8219DB50 -> 0x82119488 -> ... -> 0x82118FA0 -> 0x821185A8`, et joindre le
premier encodage PM4/Xenos ou sa branche de rejet exacte. Une run seulement si
les listes relocalisées restent indispensables ; conserver alors une
screencap Xvfb.

Validation visuelle courante : 21 PNG 1280×720 uniformément noirs, dont
`capture-020.png` inspecté humainement. `present_count=0`.

Preuves :
`artifacts/goal-playable/title-draw18-record-runtime-20260824/RESULT.md` et
`reports/cycle-1801-demo-draw18-record-and-black-screencap.md`.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Reprise précédente — cycle 1800

La liste 13 du sous-enfant post-START est jointe exactement à
`0x820EB200` : descripteur `{0,0x1998}`, 16 records type 0,
`draw_index=0x12`, dispatch `0x82325E70`, slot 7 au LR `0x82325ED4`.

Reprendre statiquement dans `0x820EB200` sur l'entrée ABI 0x12, son handle de
draw et sa queue. Les 24 draws typés n'ont encore produit aucune ressource
présentable (`present_count=0`) et aucune image. Le prochain gate doit traiter
la screencap comme un livrable vérifié : sortie du run courant, classification
automatique, puis inspection visuelle.

Preuves : `artifacts/goal-playable/title-list13-runtime-20260824/RESULT.md` et
`reports/cycle-1800-demo-list13-renderer-consumer.md`.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# SESSION_ROTATE — reprise après cycle 1798

La chaîne enfant est fermée jusqu'au type 4 du sous-enfant :
`0x2E3F8C50/type5/index 0x42 -> 0x2E3F1350/type4/list 13`. Aucun raw VM n'est
émis par cette branche au tick 3001.

Reprendre statiquement sur `list_index=13 -> 0x82326420 -> record ->
renderer`. Voir `NEXT.md` et
`artifacts/goal-playable/title-grandchild-frame-runtime-20260824/RESULT.md`.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.

# Reprise précédente — après cycle 1797

Ne plus chercher un `execute_raw` direct du premier tick de
`0x2E3F8C50` : le run naturel l'exclut. Ce tick construit le sous-enfant
`0x2E3F1350`. Reprendre sur la seconde factory au tick 3001, en journalisant
seulement ses `B/D`, frame 0 et effets VM/callback corrélés.

Voir `NEXT.md` et
`artifacts/goal-playable/title-child-first-effect-runtime-reuse-20260824/RESULT.md`.
Les statuts restent `frontend=false`, `mission=false`, `terminal=false`,
`supported=false`.

# Reprise autonome — AC6 démo PAL native

## Checkpoint — jointure shader mapparts (24 août 2026)

Chaîne canonique fermée pour 169/170 objets :
`material+0x00=0x30000010 -> 0x8296BF80 -> descripteur NSXR ->
vsCstCT + psCT`. Le format `06/13`, les textures et `NU_FLAG1/2` sont consommés
séparément. La paire HDR homologue est sous `0x30040010`, mais son writer à
`material+0x14` reste inconnu. L'exception `0x30000090` est absente des NSXR.

Reprendre par `Function_822E8488`, `Function_822EDF60`, puis le bit `0x80`.
Script : `tools/verify_demo_shader_selection.py`. Aucun runtime.

## SESSION_ROTATE — cycle 1795 (24 août 2026)

Trois gates sont fermés dans cette session : réconciliation 1793, consumer
1794, puis promotion/premier tick 1795. Reprendre dans une nouvelle session.

`0x82323BB8` promeut la tête `A+0xE4` vers `A+0xF4` au début du tick et appelle
récursivement les nouveaux enfants de `A+0xE4` à la fin de la même invocation.
L'enfant post-START reçoit donc naturellement son premier tick ; la
publication différée n'est plus la frontière.

Le gate suivant est strictement statique : depuis ce premier tick enfant,
qualifier la frame initiale, l'opcode `MovieMemory` ou callback natif produit,
puis son premier consumer persistant hors du contrôleur. Ne lancer un runtime
que pour une ambiguïté causale nommée et bornée. `supported=false`.

Preuves : `reports/cycle-1795-demo-moviecontroller-promotion-first-tick.md` et
`artifacts/goal-playable/title-child-promotion-static-20260824/RESULT.md`.

## Checkpoint autoritaire — enfant pending, promotion active ouverte (24 août 2026)

Le corps canonique `0x82323468..0x823235CF` enregistre le
`swg::MovieController` enfant puis l'insère dans `A+0xE4/A+0xE8`. Il ne le
publie pas dans `A+0xF4`. La route de tick déjà qualifiée appelle
`0x82323BB8(A+0xF4)` pour faire vivre la timeline UI scriptée et traiter les
offsets de `MovieMemory`.

Reprendre statiquement par les writers de `A+0xE4/A+0xE8/A+0xF4`. Fermer le
prédicat de promotion pending→active et le premier tick `0x82323BB8`, ou
nommer par preuve négative bornée le propriétaire alternatif. Aucun runtime
avant cette réduction. `supported=false`.

Preuves : `reports/cycle-1794-demo-child-controller-consumer.md` et
`artifacts/goal-playable/title-child-consumer-static-20260824/RESULT.md`.

## Checkpoint autoritaire — MovieController enfant observé, publication ouverte (23 août 2026)

Au tick 3001, sous le vrai Title, le callsite enfant `0x8232356C` appelle
naturellement la factory `0x820D18C8`, qui initialise et retourne un
`MovieController` cohérent avec `A`, `B=A+8`, `D`, `s=-1` et son
`MovieMemory`. Cette preuve ferme l'existence d'une reconstruction post-START,
mais l'observateur s'arrête au retour de la factory : publication racine,
remplacement, `EndMode`, listener Title et `manager+0x18` restent ouverts.

Reprendre uniquement en statique dans le projet Ghidra canonique
`ace-combat-6-demo`. Qualifier le corps contenant le callsite
`0x8232356C`, son prédicat, la source de `D`, puis le consumer/publicateur du
résultat. Ne lancer une nouvelle trace qu'après avoir nommé une ambiguïté
causale irréductible et son `done_when`. Le gate frontend exige toujours un
état guest persistant et une frame visible post-transition ;
`supported=false`.

Preuves :
`reports/cycle-1793-demo-post-start-child-moviecontroller.md` et
`artifacts/goal-playable/title-post-start-moviecontroller-join-runtime-20260823/RESULT.md`.

## Checkpoint autoritaire — START republie du contenu, transition encore ouverte (23 août 2026)

Le cadrage `M102=0 / lookup 0x0B -> EndMode` est supersédé et réfuté. START
republie une plage issue du chemin storage/descripteur ; la frame mono-entrée
type 4 atteinte ne fait que sélectionner liste et matrice puis appeler la
méthode virtuelle `CSwgRenderer::slot7` à `0x820EB200`. Elle ne contient ni
commande EndMode, ni accès au mode manager. Les 2 351 frames brutes du SWG
contiennent onze éléments type 6 mais aucun payload littéral `0xE04`; la
relocation vers le buffer heap reste à prouver.

Reprendre statiquement par trois bords distincts : carte SWG brut vers
heap/descripteur, producteur des arguments republiés à START, et état
persistant de la vraie instance `CModeTaskTitleDemoOffline` pouvant choisir
Loading/GameDemoOffline ou armer `manager+0x18`. Ne lancer aucune trace tant
qu'une arête causale nommée n'a pas survécu à ces trois passes. Preuves :
`artifacts/goal-playable/title-swg-post-start-static-correction-20260823/RESULT.md`,
`artifacts/goal-playable/title-frame-descriptor-static-20260823/RESULT.md` et
`artifacts/goal-playable/title-parallel-matrix-consumer-static-20260823/RESULT.md`.

## Gate fermé — logo naturel et mode manager qualifié (23 août 2026)

Le rendu naturel du logo Namco est fermé par deux cold runs byte-identiques :
handle `0x0E000059`, Q1 guest-owned non nul, samples utiles et readback
1280x720 reconnaissable. Reprendre après cette époque, sans optimiser les
couleurs ni répéter la fenêtre logo.

La mécanique `CTaskModeManager` est prouvée dans le projet Ghidra
canonique. `0x827435F8` contient le manager, `0x8218EA88` écrit sa factory
`+0x10`, et `0x82190B18` construit/publie/insère le nouveau mode lorsque
`+0x18` est armé. Aucun shim ni écriture de champ guest. Reçu :
`artifacts/goal-playable/frontend-mode-manager-canonical-ghidra-20260823/RESULT.md`.

Le prompt « PRESS START » arrive beaucoup plus tard que le logo. La classe
`CModeTaskGameDemoOffline` est propre à la démo et son absence des prototypes
retail est attendue. Les checkpoints plus anciens ci-dessous sont historiques
et ne définissent plus le gate actif.

## Checkpoint le plus récent — liste SWG imbriquée bloquée sur draw0 (23 août 2026)

Le replay ciblé 190..320 observe une frame imbriquée valide mais unique :
`list_index=0`, `list_count=1`, `draw_index=0`, slot `0x57`. Aucun record
`draw2/0x59` n'atteint le renderer, tandis que l'owner parent avance jusqu'à
la frame 31. Reprendre statiquement par le producteur/script de la frame et de
`element+8` qui doit sélectionner la liste du wordmark. Ne pas répéter ce run
ni modifier scheduler, renderer ou ressources. Preuve :
`artifacts/goal-playable/brandlogo-draw2-selector-runtime-20260823/RESULT.md`.

## Checkpoint le plus récent — blend titre fermé, handle `0x59` non tiré (23 août 2026)

Le renderer titre compose maintenant avec le blend Xenos qualifié et conserve
la chaîne naturelle RT0/resolve/writeback; les tests ciblés passent. Le run de
400 ticks produit des frames évolutives mais encore uniformément bleues. Le
payload Namco est chargé et enregistré naturellement sous le handle `0x59`,
alors que la timeline atteinte ne tire que `0x57` et brièvement `0x58`.

Reprendre uniquement par le sélecteur/producteur de `draw_index=2` et son
raccord au worker/PM4. Aucun nouveau runtime global, remapping de ressource,
draw forcé ou pixel synthétique n'est justifié. Preuves :
`artifacts/goal-playable/title-blend-runtime-20260822/RESULT.md` et
`artifacts/goal-playable/brandlogo-resource-entry-runtime-20260822/RESULT.md`.

## Checkpoint le plus récent — BrandLogo atteint `0x821B4D80` (22 août 2026)

Reprendre depuis
`artifacts/goal-playable/brandlogo-consumer-runtime-20260822/`. Le record
BrandLogo type 1 et sa ressource passent naturellement le consommateur, la
queue et le worker jusqu'au constructeur guest `0x821B4D80`, mais le ring
reste à zéro soumission. La prochaine reprise est statique : identifier son
buffer/cursor/flush puis le point partagé exact vers le writer PM4/Xenos. Ne
pas synthétiser ressource, record, commande, draw ou pixel.

## Checkpoint le plus récent — records BrandLogo publiés (22 août 2026)

Reprendre depuis `artifacts/goal-playable/acc-resource-runtime-20260822/`.
Le guest franchit naturellement `0x820EB200 -> 0x820EA9A0 -> 0x82095DF0 ->
0x821DEED8`; les slots ressource et la géométrie sont valides, et les records
`0x70` sont liés à `0x8281EAF0`. Le runtime reste noir, sans soumission ring
ni draw non-bootstrap. La prochaine reprise est statique : qualifier le
consommateur aval de cette liste et son raccord Xenos. Aucun mapping ACC,
record, draw ou pixel synthétique.

## Checkpoint courant — callback SWG atteint mais non armé (22 août 2026)

Reprendre depuis `artifacts/goal-playable/swg-callback-arm-20260822/`. La
vtable `0x820061FC/+0x14` appelle bien `0x820CDF30`; `this+8=1`, mais
`this+9=0`, le garde `0x826DFC48=0` et les bitsets d'état logique/action
`base+0x23980/+0x23988/+0x2398C/+0x23990` restent nuls. Le frontbuffer reste
noir et `normal_draws=0`. Les écrivains `0x821DE990` et `0x821DE6E0` sont
qualifiés; le prochain travail est statique : qualifier leur événement
SWG/script ou le consommateur de la garde globale. Ne pas synthétiser les
bits, le callback, un record ou des pixels.

## Checkpoint courant — entrée de jeu non publiée (22 août 2026)

Le slice `task-loading-publication` est négatif et fermé : les fonctions
`0x8218CE20`, `0x8218CCD0`, `0x82259D10`, `0x82259E18`, `0x82259FF8` et
`0x821929A8` ne constituent pas une publication de mode. La prochaine reprise
statique doit suivre la factory amont vers `0x8217C678` et l'insertion de la
tâche; aucun appel forcé, shim ou rendu synthétique n'est autorisé. Voir
`artifacts/goal-playable/task-loading-publication/RESULT.md` et `BLOCKER.md`.

La passe statique `game-entry-static` ferme négativement la jonction
`START → CModeTaskGameDemoOffline`: `0x8217C678`/`0x8217C4D8(-3)` et les
constructeurs `CX360UnitManager` restent non atteints. Le target
`0x820D32D0` n'est qu'un thunk virtuel `+112`, donc aucun remplacement ou
appel direct n'est autorisé. Le prochain slice est la terminaison de
`CTaskLoading` et la publication du mode manager autour de
`0x8218CE20`, `0x82259E18/0x82259FF8`, `0x821929A8`. Preuves :
`artifacts/goal-playable/game-entry-static/RESULT.md` et `BLOCKER.md`.

Dernier slice statique négatif : `0x8218BFB0` sélectionne des tables de
providers et `0x8219BFB0` est un load dans `0x8219BF40`; aucun lien qualifié ne
joint le payload SWG `brandLogo` au descripteur ACC. Ne pas ajouter de mapping
synthétique ni peindre le logo côté hôte. Reprendre au constructeur/provider
ACC, preuve `artifacts/goal-playable/acc-guest-join-next/RESULT-final.md`.

Le follow-up owner/r8 confirme que le slot AVI `0x82165CC0` et le writer type
1 sont connus, mais que ni le receiver `base+0xC` ni le producteur `r8` ne sont
publiés par le guest. La reprise doit rester statique; aucun record forcé,
shim ou rendu host n'est autorisé. Preuve :
`artifacts/goal-playable/owner-r8-followup/RESULT.md`.

## Checkpoint le plus récent — logo Namco identifié, non encore visible (22 août 2026)

Le boot-logo est borné statiquement à la route SWG
`0x820E8F90 → 0x820EA4A8 → 0x821728C0`. Son payload disque est désormais
qualifié : `DATA.TBL[170/171]` contient `FHM → FHM → SWG "brandLogo"` de
118244 octets avec huit NTXR soeurs portant `GIDX=0x08000000`. Le tableau SWG
fixe le mapping `texture_id 2 → 003_NTXR.ntxr`, décodé comme le mot-symbole
rouge `namco®` sur fond blanc ([artefact visuel](artifacts/goal-playable/brandlogo-swg-refs/png/0002_1280x720_fmt14_mip1.png)). La provenance
immédiate de l'ACC est qualifiée (`0x821A02C0 → 0x8219E768` et branche
ressource `0x821A0180 → 0x8219F080/0x8219F1C0 → 0x8219E428`), tandis que son
raccord au chargeur PAC/FHM, au NTXR exact et aux handles
`0x0E000057..0x0E00005E` reste la frontière active.

Ne pas toucher au renderer : sa chaîne `nuTexture → SWG draw → RT0 → resolve`
est déjà réutilisable, tandis que le runtime n'a aucun draw non-bootstrap et
aucun readback non noir. Reprendre par le producteur ACC et le mapping
`handle → ressource`, puis seulement valider une image 1280×720 réelle.
Preuves : `artifacts/goal-playable/namco-logo-route/RESULT.md`,
`artifacts/goal-playable/swg-payload-static/RESULT.md`,
`artifacts/goal-playable/brandlogo-swg-refs/RESULT.md` et
`artifacts/goal-playable/brandlogo-loader-join/RESULT.md`.

La passe complémentaire des clés `0xCB..0xCF` est fermée négativement : elles
sont seulement normalisées en `DPL::[cb..cf,0]` puis envoyées vers
`0x8219E768`; aucun raccord vers `0x8219E428`, PAC/FHM/DATA.TBL ou SWG n'est
qualifié. Reprendre par le producteur ACC consommé par `0x8219E580`.
Preuve : `artifacts/goal-playable/logo-key-registry/RESULT.md`.

La dernière passe a séparé définitivement le callback de démarrage du
producteur ACC : `0x821728C0` délègue à l'owner et avance l'état, tandis que
les consommateurs `0x8219E580`/`0x82127D40` restent sur des vtables distinctes.
Aucun write `+0x18/+0x1c/+0x20` ni identité de table source ne joint encore
le payload `brandLogo`. Reprendre par ce constructeur/registre ACC; garder le
renderer fail-closed et ne pas fabriquer le logo. Preuve :
`artifacts/goal-playable/acc-guest-join-next/RESULT-followup.md`.

## Checkpoint le plus récent — owner/r8 (22 août 2026)

Reprendre depuis `artifacts/owner-r8-static/RESULT.md`. Le runtime borné a
terminé à 3036 ticks (résultat `max_ticks`) et n'a produit aucun PM4 : seul
`0x820FEFA8` est observé, avec un record de type zéro. La frontière statique
est désormais le contrat d'owner AVI (`0x8200B6FC+0xC → 0x82165CC0`) et la
valeur-flow du service `0x82386C58` slot `+0x08`; les cinq ressources exigées
par le writer type 1 restent non qualifiées.

Deux writers titre (`0x820B3E80`, `0x82321E18`) écrivent `+0xE8`, mais leur
join vers l'owner `0x820D29E0`, l'AVI et les records reste indécidable.

Le provider est maintenant séparé statiquement de la table de records :
`0x82165AF8` parcourt `DAT_826F6188`; `0x821080D0` transforme les cinq clés
du payload en IDs pour `DAT_826F6124`; `FUN_821ee130` ne produit pas ces clés.
Reprendre par la population des entrées de `DAT_826F6188` dans
`0x82165040`/la voie jumelle, puis par la cible du service `0x82386C58`.

La passe AVI finale classe `0x82374AD0` comme initialiseur de la base
`0x82731A30`; les constructeurs écrivent `0x8200B6FC` à `objet+0xC`, sans
publication qualifiée vers `0x82165CC0`. La trace ciblée `owner-window-300` (stores 220:223) donne
192 PRESENT mais zéro soumission ring/PM4 et aucun owner ciblé; ne pas ajouter
de shim ni modifier le renderer.

Ne pas toucher au renderer/Xenos ni au code généré avant fermeture de cette
frontière. Les validations déjà passées sont codegen-ON CTest `26/26`,
codegen-OFF build/CTest `27/27`, et installation au préfixe portfolio sans
`bin/bin`. Le prochain travail autorisé est statique; une trace ciblée n'est
permise que si une ambiguïté causale précise subsiste.

## Dernier gate statique limité — portée de la sonde IB

La sonde historique exclut les IB bootstrap par sa plage codée en dur; son
silence ne dit rien sur leur producteur. Le slice statique est épuisé. Reprendre
avec une watch native d'un tick, exclusivement sur
`0x16AE0980..0x16AE0A40`, selon les trois issues préétablies dans
`artifacts/ib-bootstrap-producer-static-gate/BLOCKER.md`.

## Dernier gate limité — bootstrap de 24 `PointList`

Le lot précoce `0x16AE0980` suit une écriture de masque couleur nul et ne
contient pas de payload vertex observé. Il n'est pas un writer RT0 promouvable,
sans permettre d'exclure un effet interne Xenos. Reprendre par le producteur
de la chaîne IB `0x1685A000 → 0x16AE0980`, statiquement.

## Dernier gate fermé — transport RT0 vers lecture

Le draw normal qualifié transmet son buffer RGBA à la résolution neutre avec
le draw de copie et le PRESENT. Le harness EDRAM couvre les deux types de
source et refuse une sortie modifiée; compilation et tests passent. Cela ne
prouve aucun pixel non noir. Reprendre par le premier writer RT0 qualifié
capable de produire une couleur non nulle, statiquement.

## Dernier gate fermé — sélection du draw RT0

Les rectangles normal et copie qualifiés sont maintenant conservés avant la
déduplication du cache de pipelines, ce qui alimente les consommateurs déjà
présents. Le test de contrat dédié, la compilation et les tests configurés
passent. Reprendre par le contrat synthétique de lecture RT0; ne toucher ni
aux shaders ni au runtime complet.

## Dernier gate limité — owner du slot virtuel

`0x820D29E0` est le slot `+0x0C` de la vtable `0x820064D8`; ses constructeurs
ne produisent pas l'owner `+0xE8`. L'appelant du constructeur est lui-même
indirect. Reprendre en trouvant le site d'appel du slot et en remontant ses
deux arguments, sans runtime.

## Dernier gate limité — installation du contexte SWG

`0x82324188` copie `owner + 0xE8` vers le champ contexte de l'interpréteur et
appelle son slot `+4`; il ne construit pas l'objet ni sa vtable. L'unique
appelant `0x820D29E0` transmet son second argument comme owner. Reprendre par
les appelants de `0x820D29E0`, statiquement.

## Dernier gate clos — handlers SWG post-START

Les indices 12, 19, 26 et 68 de `DAT_8264CEA8` sont des thunks de vtable sur
`context + 0x0C`; l'indice 50 lit un mot avant d'appeler un autre slot. Aucun
CFG ne relie directement ces handlers au sélecteur global. Les cinq slots à
qualifier sont `+0x64`, `+0x80`, `+0xA8`, `+0xC8` et `+0xCC`.

`SESSION_ROTATE` a été émis après ce troisième gate fermé du cycle.

## Dernier gate clos — consommateur de table SWG

`0x82325160` dépile les mots à `context + 0x14` puis appelle
`DAT_8264CEA8 + mot * 4`. Les cibles post-START sont les indices 12, 19, 26,
50 et 68. Le CFG et les xrefs excluent une arête directe de ce consommateur
vers le sélecteur global. Reprendre par les handlers indexés, statiquement.

## Dernier gate limité — tables SWG post-START

Les xrefs des neuf cibles post-START tombent uniquement dans des tables de
données; six cibles n'ont aucune fonction Ghidra. Aucun chemin direct vers le
sélecteur global n'est établi, sans pouvoir exclure un chemin indirect.
Reprendre par le consommateur et le layout de `0x8264CED8..0x8264CFB8`, sans
runtime.

Gate global/vtable limité: 24 xrefs directs, tous READ; aucun store direct.
Le script existant échoue structurellement car il attend un champ receiver
intermédiaire. Reprendre par lecture Ghidra big-endian directe de
`0x82386C10 -> object+0 -> vtable+0x04/+0x34`, puis comparer les deux cibles au
manifeste codegen. Aucun runtime.

Gate chaîne statique fermé: `0x82386C10 -> 0x82386C0C -> vtable 0x82008E50`;
slot 1 `0x820FEED8`, slot 13 `0x820FEF70`, deux cibles présentes au manifeste.
La famille `0x821075A0` n'est pas manquante. Reprendre au premier autre appel
indirect atteignable sans vtable/cible déjà qualifiée; aucun runtime.

Prochaine famille: `0x82107870/0x82107A00` déjà éliminées. `0x821154C0` reçoit
le retour de `0x82220670` et appelle slot 20; `0x82115530` appelle slot 21.
Lire d'abord `artifacts/resource-next-indirect-family-gate/slot20-producer.txt`
et le log associé, sans relancer Ghidra, puis borner la vtable. Aucun runtime.

## Dernier gate clos — divergence virtuelle SWG au START

La capture native unique avec START au tick 3000 donne 22 appels sélectionnés
au tick 3000 et 69 au tick 3001. Le site `0x823251B8` atteint des cibles
propres au tick 3001, notamment `0x82324548`, `0x82324440` et `0x820D7768`.
`0x8218AB98` n'est pas relevé aux trois sites instrumentés, sans conclusion
hors de ce périmètre. Reprendre par le CFG et les xrefs des cibles exclusives
au tick 3001; ne pas relancer le runtime auparavant.

## Dernier gate clos — dispatcher SWG native borné

Le dispatch `0x820EA238` mène statiquement au chemin neutre
`0x8217C890 → 0x8218AB98`, jamais directement au setter mission global. La
décision START/neutre appartient au film SWG. Après rotation, reprendre avec
la capture ciblée de la seule cible virtuelle et de son contexte, selon les
critères de `artifacts/title-dispatcher-selection-static-gate/decision.md`.

## Dernier gate limité — collision de l'offset titre/sélection

`0x8218AB98` met à jour `this + 0x70` du mode titre; le setter mission accède
à `PTR_DAT_823c27e0 + 0x70`. Ces objets ne sont pas reliés statiquement.
Reprendre par les xrefs du dispatcher titre dans `ace-combat-6-demo`, sans
inférer une identité depuis le seul offset et sans runtime anticipé.

## Dernier gate clos — vocabulaire compact du loader

Le loader `0x82278F78` est un décompresseur bitstream (tables `+0x64/+0x68`,
réservoir `+0x48/+0x50`, source `+0x38`, destination `+0x3C`). Reprendre par
la provenance statique de l’entrée compacte du clip titre; ne pas relancer le
runtime des opcodes.

## Dernier gate clos — décodage borné du flux titre

Le parser local explique 23/23 appels box entre `0x2DCB2438` et
`0x2DCB2680`; le premier mot inconnu est `0x2DCB2448 = 0x1A`. Reprendre par
le CFG et le bit-reader de `0x82278F78` pour qualifier le vocabulaire compact,
sans nouvelle capture runtime.

## Dernier gate clos — chargeur de flux relié au contexte titre

Le chargeur écrit `0x2DCB2000..0x2DCB3FFF` au tick 2435; les PC titre au tick
3001 tombent dans cette plage. Reprendre avec un parser isolé de la fenêtre
`0x2DCB2440..0x2DCB2680`, jamais avec un nouveau runtime.

## Dernier gate clos — frontière statique du branchement de film

Le film est un flux heap interprété par `0x823246C0`; son chargeur
`0x82278F78` décompacte une entrée non reliée statiquement au contexte titre.
Le lien requiert donc une capture très bornée, uniquement chargeur/destination
et PC/buffer de l’interpréteur titre autour de 2429–2435.

## Dernier gate clos — consommateurs statiques du triplet sélectionné

La table de commandes du titre associe `GetCurrentMission` à `0x820EA550` et
`GetCurrentLevel` à `0x820EA598`; leurs CFG appellent les deux getters démo
qualifiés. Les xrefs PPC directes sont absentes mais ne réfutent pas ce
dispatch script. Reprendre par le bytecode/branchement du film après ces
natives, sans runtime.

## Dernier gate clos — écrivain direct de mission au START écarté dans la fenêtre

La décompilation démo place le slot mission sélectionné à `0x823C2F14` pour
l'index observé zéro. START injecté au tick 3000 le laisse stable jusqu'au tick
3004; une surveillance de store sur ce seul slot ne produit aucun événement.
Les appelants directs du setter ne l'écrivent pas dans cette fenêtre. Reprendre
par les consommateurs statiques des getters mission/niveau dans le corpus
`ace-combat-6-demo`, sans nouveau runtime.

## Dernier gate clos — producteurs statiques de mission bornés

Neuf appelants directs du setter mission sont établis, mais aucune voie n'est
attribuée au START et le setter niveau reste indéterminé. Reprendre par une
capture START limitée au PC appelant et à `r4` du setter, sans trace globale.

## Dernier gate clos — bornes START dans le XEX démo

Ghidra `ace-combat-6-demo` confirme les quatre observables dans le même XEX.
Mission getter et setter partagent le champ indexé `+0x6C4`; niveau lit
`+0x6D0`. Reprendre par les producteurs statiques de l'index et de ces champs,
sans runtime.

## Dernier gate clos — entrée du codegen démo qualifiée

Le build codegen-on emploie `demo-game-file/extracted/stfs-root/Default.xex`,
son manifeste Ghidra démo et le projet séparé `ace-combat-6-demo`. Reprendre
par les bornes de fonctions démo des observables START, sans croiser le corpus
retail.

## Dernier gate clos — réconciliation du corpus Ghidra canonique

Le corpus PAL contient bien ses fonctions, mais les adresses issues du code
généré du demo y appartiennent à des corps incompatibles. Elles ne sont pas
transférables entre entrées XEX. Reprendre par la qualification statique de
l'entrée réellement utilisée par le codegen demo, sans réutiliser les noms ou
la sémantique du corpus PAL.

## Dernier gate clos — cible du setter de mission au START réfutée

Le probe START borné voit `gs=0x82774B00` mais `gs+112=0x8201DFDC` aux ticks
3000–3004; cette dernière valeur est une table de méthodes. L'absence de store
sur `0x82775234` ne prouve rien sur `0x82171988`. Reprendre par la provenance
statique de `gs+112` et les champs réellement lus par les getters; aucun
runtime supplémentaire avant cette cible.

## Gate remplacé — provenance statique de `gs+112`

Le blocage d'import PAL est résolu : le corpus possède ses fonctions, mais les
adresses de la démo y sont non correspondantes. La provenance reprend dans le
projet `ace-combat-6-demo` après qualification des bornes.

## Dernier gate clos — valeurs SWG après START

Au START, les getters de film retournent `0 / 0 / 2`; le monde SWG reste
identique et `menu_endMode` n'est pas appelé. Les substitutions ciblées déjà
faites ne changent rien. Reprendre par le producteur statique de l'état lu par
le film, avant toute capture; ne pas revenir au renderer.

## Dernier gate clos — sélecteur SWG post-START du titre

Le seul écrivain du monde SWG est `0x820CE368`; tous les wrappers de film ne
font que déléguer à ce monde. Le choix attract/START demeure dans le bytecode
ou ses valeurs consommées. Reprendre avec une fenêtre runtime limitée aux trois
getters, au contexte et à l'appel suivant; ne pas tracer le renderer.

## Dernier gate clos — avancement neutre de l'attract title

Le callback neutre SWG atteint `0x8218AB98`, écrit l'état titre à `+0x70`, puis
l'état 2 mène à une transition de tâche. START ne le déclenche pas : il utilise
d'autres handlers et supprime l'attract. Reprendre par le sélecteur SWG de ce
choix, statiquement avant toute capture; aucun draw RT0 nouveau n'est établi.

## Dernier gate clos — consommateur statique du compteur de titre

`0x820D3AC8` n'est référencé que par sa vtable, donc le consommateur de son
compteur heap ne se remonte pas par appel direct. Le burst de VM ne produit
aucune transition persistante connue. Reprendre par le premier contenu RT0 non
nul, avec slice statique avant toute capture bornée.

## Dernier gate clos — récepteur virtuel du START pendant le titre

Le slot titre `+0x70` vise `0x820D3AC8`, qui évalue le slot `+0x5C` d'un
contexte et décrémente un compteur guest. Reprendre par le consommateur de ce
compteur ou du résultat de VM, d'abord en statique; aucun menu ni draw non nul
n'est encore prouvé.

## Dernier gate clos — dispatch START pendant le titre

Pendant le titre, START atteint le thunk `0x820D32D0`, qui dispatch le slot
virtuel `+0x70` de `r3`. Les références statiques ne révèlent pas la cible.
Reprendre par son récepteur, d'abord statiquement puis, seulement nécessaire,
par une capture bornée de l'objet/vtable/slot au tick de START.

## Dernier gate clos — producteur PM4 du draw RT0

`0x821B55C0` produit la commande draw et `0x821B9BC8` la publie au WPTR; aucun
writer CPU EDRAM n'existe dans cette chaîne. Reprendre par la progression guest
vers un draw à contenu non nul, sans runtime global ni pixel synthétique.

## Dernier gate clos — reflection varying du draw normal

Le geometry shader de test perd les varyings, mais `interpolator0` et `color0`
du rectangle atteint sont nuls : ce n'est pas la cause de la noirceur. Reprendre
par le premier écrivain non nul de RT0/EDRAM, sans runtime.

## Dernier gate clos — interface vertex du draw normal

Le record de draw ne transporte aucun fetch, mais le descripteur shared-memory
lié séparément couvre la plage guest du rectangle normal. Reprendre par
l'interprétation statique shader/format/lane de ces octets, sans runtime ni
pixel synthétique.

## Dernier gate clos — disposition d'installation native

L'installation CMake temporaire produit `bin/ac6-demo-recomp` et la
configuration sous `share/ac6-demo-recomp/config`, sans `bin/bin`. C'est une
preuve de disposition, pas de comportement guest.

## Dernier gate clos — intégration locale du bridge natif

Le build `recompilation/ace-combat-6-demo/build` est à jour et ses 27 CTest
passent avec l'audio dummy. Le résultat couvre l'intégration locale, sans
preuve de frontend, mission ni image non noire.

## Dernier gate en pause — producteur de `VdGlobalDevice+0x4084`

`0x821C64E8` initialise le device, `0x821C6400` enregistre le callback
`0x821B9710`, puis le constructeur publie le device dans `VdGlobalDevice`.
La provenance du champ interne `+0x4084` reste hors de l'image titre, dans
l'ABI Vd externe. Ne pas la remplacer par une hypothèse runtime.

## Dernier gate clos — interface du callback indirect graphique

Le `bctrl` de `0x821C5190` cible le pointeur conditionnel
`VdGlobalDevice+0x4084` sous spinlock `+0x4130`, et transmet un record local
de six mots. La cible reste inconnue ; ne lui attribuer ni effet PM4 ni pixel
sans un nouveau gate statique de provenance du pointeur.

## Dernier gate clos — contrat CPU du callback XAudio

La cible locale `ac6-demo-xaudio-callback-cpu-contract-tests` compile et son
CTest ciblé passe. Cela valide le contrat de sélection/rejet local seulement,
sans promotion de la chaîne audio ou de l'état gameplay.

## Dernier gate en pause — écrivain non nul du record render

Le producteur `0x820FF710` remet le champ de contrôle du slot à zéro et avance
l'index ; le worker `0x820FFCA0` recopie le record. Aucun appel direct vers le
producteur n'existe dans le projet canonique, donc ses appelants ne sont pas
qualifiables par ce slice. Reprendre seulement avec un outil statique de
provenance de dispatch indirect ou de sauts entrants, jamais par runtime.

## Dernier gate clos — faux consommateur de payload render

`0x820FFCA0` passe son record de 96 octets à `0x820FEFA8`, mais le p-code de
ce dernier écrase `r3` par le contexte `0x82327104` avant toute conservation
de l'argument. Il ne lit pas le payload. Reprendre par les écrivains statiques
de champs non nuls du record, en commençant par les appelants de `0x820FF710`;
aucun runtime.

## Branche en pause — consommateur fournisseur `+0x20`

Les deux slots de copie écrivent bien l'état à `fournisseur+0x20`, mais les
1 066 accès D-form candidats à base `r3`/`r4` ne permettent pas de conserver
l'identité du fournisseur. Ne pas lancer de runtime. Reprendre seulement avec
une procédure statique existante de suivi producteur-consommateur du pointeur.

## Dernier gate clos — corps des slots `+0` de fournisseurs

Les trois slots sont des feuilles : `0x822F2F20` retourne `E_NOTIMPL`; `0x822FFCC8` et `0x822FFDB8` copient respectivement 16 et 64 octets de `r4` à `r3+0x20`, puis retournent zéro. Ni liaison shader, ni MMIO/ring, ni appel n'est présent. Reprendre par le premier lecteur de l'état fournisseur `+0x20`, sans runtime.

## Dernier gate clos — vtables des fournisseurs non-bootstrap

Les sept vtables RTTI `ShaderParameter` candidates se réduisent à trois cibles de slot `+0` : `0x822F2F20`, `0x822FFCC8` et `0x822FFDB8`. L'initialisation correspondante est une zone sans frontière de fonction reconnue; ne pas utiliser son pseudocode. Reprendre par le p-code de ces trois cibles, sans runtime.

## Dernier gate clos — effet de fournisseur `0x822E0B90`

`0x822E0B90` est une dispatch en queue vers la slot `+0` de la vtable du fournisseur; il ne produit aucun état GPU local. Reprendre par les vtables concrètes des fournisseurs retournés par `0x822E0B10`, sans runtime.

## Dernier gate clos — lecteurs du profil global non-bootstrap

`0x822DEBD8` passe `0x829D0800` à `0x822E9BD8`, qui le conserve dans `r29` et transforme ses fournisseurs aux offsets `+0xc/+8/+0x10` par trois appels à `0x822E0B90`. Reprendre seulement par le CFG de ce callee commun; aucun runtime.

## Dernier gate clos — provenance statique du profil non-bootstrap

Le slice PPC de l’unique appel établit que `0x822E9A18` reçoit `r3 = 0x829D0800`; l’adresse est un global de profil, et non un retour de `0x8232710C`. Reprendre par les lecteurs statiques de ce global et de ses champs `+8/+0xc/+0x10`, sans runtime.

## Dernier gate clos — consommateur du profil de fournisseurs non-bootstrap

`0x822E9A18` conserve trois fournisseurs résolus, mais l’accesseur `0x8232710C` est un stub vide partagé par de très nombreux appelants. Il ne permet pas de rattacher statiquement cet enregistrement à un consommateur shader/PM4. Reprendre uniquement par la provenance ABI ou le stockage de l’adresse retournée; aucun runtime n’est autorisé sur cette seule question.

## Dernier gate clos — slot de résolution `+0x2c`

La vtable de `Shader::ShaderContextXenon` cible `0x822F2B88` à `+0x2c`. Ce callee sélectionne un fournisseur dans deux listes par identifiant et retourne son pointeur; il ne charge pas de shader ni ne construit un paquet GPU.

À la reprise, suivre seulement le premier consommateur des trois pointeurs de fournisseur écrits par `0x822E9A18`.

## Dernier gate clos — sélection non-bootstrap

Le CFG démo PAL `0x822E3858 → 0x822E9948` initialise seulement cinq profils; il ne sélectionne aucun shader pixel. Le résolveur `0x822E0B10` traverse le gestionnaire global puis une slot virtuelle `+0x2c`, qui est la frontière exacte à reprendre.

Le prochain gate est uniquement l’identification statique de cette vtable et de son callee; commencer par RTTI/vtables/xrefs, sans runtime.

## Dernier gate clos — origine EDRAM / readback

Le readback du draw normal démo atteint est nul avant la matérialisation EDRAM. Le resolve certifie ensuite l’égalité exacte entre cette entrée et les octets tuilés avant writeback, tandis que l’oracle isolé démontre qu’un motif non nul traverse bien la matérialisation. Ne pas modifier copie, conversion ou writeback pour corriger cette noirceur.

À la reprise, ouvrir uniquement le gate de sélection du pixel shader non-bootstrap décrit dans `NEXT.md`, avec sources natives puis xrefs/CFG démo PAL. Aucun runtime complet n’est autorisé pour cette question.

## Gate fermé — enveloppe de soumission pilote `0x821BFBA8`

Le buffer I/O est borné à une longueur en unités de `0x200`, avec préfixe
`0x10`, copie optionnelle de `0x38` et section optionnelle de `0x600` octets.
Le wrapper `0x821A66E0` ne le décode pas avant `NtWriteFile`; aucun lien PM4 ou
WPTR n'est démontré. Garder ce format opaque et ne pas lancer de runtime.

## Gate fermé — contrat natif de soumission `0x821BFBA8`

La comparaison locale a borné deux contrats distincts : `0x821BFBA8` forme une
enveloppe transmise à `NtWriteFile`, alors que le publisher Xenos natif est
atteint uniquement par le store guest `0x7FC80714`. Les tests de ring couvrent
progression, wrap et rejet sans writeback. Reprendre au layout de l'enveloppe
pilote; ne modifier aucun chemin Xenos sur cette seule base.

## Branche en pause — provenance composée de l’en-tête

Les consommateurs `0x821BFBA8` et `0x821C57D0` sont qualifiés, mais les
chemins aval observés ne remplissent pas `device+0x3584`. Le producteur est
indécidable dans le budget statique de ce gate ; reprendre par une question
indépendante, sans runtime automatique.

## Dernier gate fermé — buffer du drain NtWriteFile

Le buffer de `0x800` octets est sur la pile de `0x821BF7D8`. `0x82327D90` y
copie seulement un en-tête de 56 octets depuis `device+0x3584`; la recherche
D-form ne trouve aucun accès direct à cette adresse composée. Aucun format PM4
n’est établi. Reprendre par les producteurs de cette région composée, sans runtime.

## Dernier gate fermé — handle du wrapper NtWriteFile

`0x821A66E0` transmet le buffer et la longueur à `NtWriteFile`, mais obtient
son handle via `FUN_8232710C`. Cette fonction est un stub vide dans l’analyse
DEMO : le handle est une frontière indirecte. Reprendre par le producteur du
buffer local de `0x800` octets dans `0x821BF7D8`, sans runtime.

## Dernier gate fermé — appels de drain de canaux

Les deux appels aval de `0x821BF7D8` sont I/O : `0x821A6530` positionne le
descripteur et `0x82337E38` lit/écrit ses métadonnées. Aucun ne publie de
batch Xenos. Reprendre par le wrapper `NtWriteFile` de ce CFG et le producteur
du handle, sans runtime.

## Dernier gate fermé — consommateur du travail en attente

`0x821BF7D8` consomme statiquement le bit `device+0x56ec:0x08` posé par
`0x821BF720`. Il sélectionne la borne de drain mémorisée, sans effacer le bit
ni écrire directement le ring/MMIO. Reprendre par `0x821A6530`, puis
`0x82337E38`, sans runtime.

## Dernier gate fermé — premier producteur PM4 non-bootstrap

Les cinq initialiseurs de `0x822E9948` sont des chargeurs de ressources : création/configuration d’objet puis trois résolutions par `0x822E0B10`. Aucun ne construit de PM4, de ring ou de MMIO. `0x822E0B10` franchit une slot virtuelle `+0x2c`; le premier consommateur de ses handles, et non l’initialiseur, est le pivot suivant.

Reprendre par cette consommation de handles, statiquement et sans runtime.

## Dernier gate fermé — effets de rendu bootstrap et transition non-bootstrap

Le bridge transforme les deux `IM_LOAD` bootstrap en commandes renderer et porte leurs identités dans les draws. Dans le binaire démo, `0x822E3858` appelle linéairement l’initialiseur bootstrap `0x822F0340`, puis l’initialiseur NSXR `0x822E9948`; il n’y a aucune sélection de shader ou émission PM4 à cette frontière. La première consommation PM4 d’un profil non-bootstrap reste à localiser statiquement.

Reprendre par les producteurs et consommateurs des cinq profils initialisés par `0x822E9948`, sans runtime.

## Dernier gate fermé — format PM4 du buffer indirect de `0x821B1D58`

Les 74 DWORDs du template forment une invalidation d’état, deux chargements immédiats de shader (sommet : 24 mots ; pixel : 9 mots) et des Type 0 vers des registres inférieurs à `0x2312`. Le décodeur natif impose précisément ces trois longueurs et son fichier de 0x8000 registres couvre tous les Type 0. Aucun changement ni runtime ne sont requis.

Reprendre par le consommateur renderer de ces shaders bootstrap et sa séparation des profils non-bootstrap.

## Dernier gate fermé — opérandes indirectes de `0x821B2BC8`

Le descripteur `0xC0013F00` porte l’adresse encodée de l’allocation de 0x2000 octets et le compte exact de 73 DWORDs retourné par `0x821B1D58`. L’allocateur physique local aligne cette adresse à la page ; les masques `& ~3` et `& 0xFFFFF` de `capture_indirect` sont donc identitaires pour ce producteur. Aucun changement ni runtime ne sont requis.

Reprendre par le contenu PM4 de ces 73 DWORDs et le rapprocher du décodeur natif.

## Dernier gate fermé — paquets écrits après le drain `0x821BA780`

`0x821B0D20` émet après le drain les quatre sélecteurs de bin Type 3 `0x60–0x63`, chacun à un opérande, tous acceptés par le processeur natif. `0x821B2BC8` écrit le descripteur indirect `0xC0013F00` à deux opérandes, exactement la forme que le bridge capture et développe. Aucun changement ni runtime n’est requis.

Après rotation, reprendre par le calcul des deux opérandes adresse/longueur de ce descripteur et le comparer à `capture_indirect`.

## Dernier gate fermé — consommateur de la publication de `0x821BA780`

Les appelants démo `0x821B0D20`, `0x821B2BC8`, `0x821C57D0` et `0x821C64E8` appellent `0x821BA780` au franchissement de leur limite de buffer. Le retour est le curseur employé pour le lot suivant. Comme le drain atteint déjà `0x821BA058 → 0x821B9BC8`, la publication WPTR est statiquement reliée à ces producteurs; aucun runtime n’est requis.

Reprendre par les en-têtes PM4 écrits juste après ce drain dans `0x821B0D20` et `0x821B2BC8`, puis les comparer au décodeur natif.

## Dernier gate fermé — sémantique native du segment de plage optionnel

Les deux segments de onze mots de `0x821B9DB0` sont déjà couverts par le décodeur natif : Type 0 vers `0xA31`, Type 0 vers `0xA2F–0xA30` et `WAIT_REG_MEM` Type 3 `0x3C` à cinq opérandes. Les trois registres ne sont pas opaques et le wait a sa forme qualifiée. Aucune modification, aucun test synthétique ni runtime ne sont nécessaires.

Reprendre par les appelants démo de `0x821BA780` et borner leur chemin jusqu’à la publication de ring ou à une interface indirecte.

## Dernier gate fermé — entrées de contenu de `0x821B96B8`

`0x821B96B8` consomme atomiquement le champ 64-bit `device+0x2e28` et en livre les deux moitiés à `0x821B9DB0`; les setters statiques sont `0x821B8090` et `0x821B86F8`, l’initialiseur `0x821C6928`. Les valeurs déterminent seulement le segment optionnel de plage.

Reprendre par la comparaison de ce segment avec le décodeur natif du ring, sources locales avant tout test.

## Dernier gate fermé — producteur de contenu de `0x821B9DB0`

`0x821B9DB0` réserve et remplit le buffer soumis : il retourne son adresse dans la première sortie et une longueur de 11 ou 22 dwords dans la seconde. `0x821BA780` les propage à `0x821BA058`. La seule donnée non locale de ce contenu vient de `0x821B96B8`.

Reprendre par `0x821B96B8`, uniquement en statique.

## Dernier gate fermé — source des descripteurs indirects du publisher

La chaîne statique démo est `0x821BA780 → 0x821BA058 → 0x821B9BC8`. `0x821BA780` acquiert contenu et taille par `0x821B9DB0`, réserve l’en-tête, puis `0x821BA058` remet un descripteur d’un élément au publisher. Le lien avec `0x821B2BC8` est une branche de capacité antérieure au template et ne suffit pas à identifier ce contenu.

Reprendre par les sorties mémoire de `0x821B9DB0`; pas de runtime requis.

## Dernier gate fermé — contrat natif du publisher WPTR

Le bridge valide `CP_RB_WPTR`, lit la plage circulaire depuis le dernier pointeur et traite l’indirect Type-3 `0x3F` selon le contrat de `0x821B9BC8`. La construction locale, `ac6-demo-core-tests` et `ac6-demo-xenos-command-tests` passent. Le traceur graphique est inclus dans les deux variantes de compilation ; aucune sémantique de ring n’a changé et aucun runtime n’a été lancé.

Prochaine question unique : identifier statiquement le writer Vd/MMIO qui publie le ring système vers `0x7FC80714` et sa jointure avec l’initialisation du ring.

## Dernier gate fermé — publication du ring système

Les exports lecture seule démo joignent `0x821BAAD0` à `0x821B9BC8` par les champs base, masque et index du ring. L’initialiseur appelle `VdInitializeRingBuffer`; le writer insère ses triplets indirects, publie le WPTR MMIO, puis applique `sync/eieio/sync`. Les journaux de preuve sont sous `artifacts/ring-system-publication-gate/`; aucun runtime n’a été utilisé.

Reprendre par la comparaison de ce contrat d’initialisation avec le bridge natif et ses tests locaux.

## Dernier gate fermé — contrat d’initialisation du ring natif

Le bridge reçoit la base physique et le logarithme guest, puis reconstruit la capacité en DWORDs avec la même unité que le masque statique. Il conserve aussi l’adresse de writeback du RPTR et la met à jour à la complétion. Les deux suites de ring passent ; aucun runtime n’a été utilisé.

Reprendre par le premier consommateur statique du bit `device+0x258:0x08` posé par `0x821BF720`.

## Dernier gate fermé — writer WPTR

Le writer guest du WPTR est `0x821B9BC8` : il remplit le ring défini par
`0x821BAAD0`, met à jour `device+0x2ac8`, puis écrit cet index à `0x7FC80714`
avec les barrières `sync/eieio/sync`. Les champs communs `+0x3a18`, `+0x3a1c`
et `+0x2ac8` forment la jointure statique avec l'initialisation.

Reprendre par le rapprochement de ce contrat avec le bridge natif dans
`NEXT.md`; aucun runtime n'est requis.

## Dernier gate fermé

`0x821BFBA8` soumet un buffer de canal via `NtWriteFile`, après la
réservation/synchronisation `0x821BA368 -> 0x821BA130` sur `VdGlobalDevice`.
Les arguments handle/buffer sont sélectionnés dynamiquement dans le canal et
la longueur est dérivée/arrondie. Le format reste une enveloppe pilote opaque,
pas un PM4 brut ; aucun patch natif n'est autorisé par ce constat.

Reprendre par le gate statique « publication du ring système » de `NEXT.md`.

## Périmètre

Runtime canonique : `recompilation/ace-combat-6-demo`. Cible unique :
`demo-game-file/extracted/stfs-root/Default.xex`, SHA-256
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`,
Ghidra `ace-combat-6-demo`. Ne jamais mélanger retail, projet corrected ou C++
généré. Edge sert uniquement par son source épinglé ; ne pas lancer Edge/Wine.

Objectif global : cold boot → menu visible → mission jouable → succès et échec
endogènes, avec replay déterministe, capsules et readback guest non noir.
L’objectif n’est pas atteint.

## Checkpoint actuel

Les commits fonctionnels sont `2e6228c8` puis `aa9b0534` sur `main`, poussés.
Le second retire une écriture spéculative vers `KTHREAD+0x58`. Le seul
correctif causal conservé est le contrat RPTR : le CP écrit uniquement à
l’adresse passée à `VdEnableRingBufferRPtrWriteBack`, jamais à `ptr-0x3c`.

Le runtime accepte aussi les formes PM4 atteintes `0x61/0x62/0x63` et les
valeurs dynamiques qualifiées de `EVENT_WRITE_SHD`, avec tests ciblés.

Preuve canonique :
`analysis/demo/ac6-demo-ring-readback-frontier-v1.json`.

Résultat final reproductible :

- headless et Vulkan atteignent 3 036 ticks avec START 3 000/release 3 001 ;
- 23 threads guest, tous bloqués à la fin ;
- 2 899 `VdSwap`, frontbuffer `0x1374A000`, format 6, `1280×720` ;
- second ring `0x126CA000`, 0 soumission, RPTR/WPTR 0 ;
- le batch primaire antérieur de 25 dwords est consommé intégralement dans le
  build courant (22+3, aucune attente pendante) : ne plus rouvrir la piste
  historique `RPTR=7/WPTR=25` sans nouvelle preuve de régression ;
- Vulkan : 2 modules, 0 pipeline raster, 0 normal draw, 0 writeback guest,
  aucun screenshot et aucun RGB non nul ;
- frontend, mission et terminaux restent faux.

Les notifications `VdSwap` ne sont pas un visuel. Ne promouvoir aucun
screenshot tant qu’un pipeline/readback guest non noir n’est pas joint à un
état guest qualifié.

## Validation

Depuis la racine portfolio :

```bash
export TMPDIR=/fastdata/lavaulta/tmp
cmake --build workspaces/ace-combat-6/recompilation/ace-combat-6-demo/build-codegen-on -j16
SDL_AUDIODRIVER=dummy xvfb-run -a ctest --test-dir workspaces/ace-combat-6/recompilation/ace-combat-6-demo/build-codegen-on --output-on-failure
cmake --install workspaces/ace-combat-6/recompilation/ace-combat-6-demo/build-codegen-on --prefix "$PWD/workspaces/ace-combat-6"
test ! -e workspaces/ace-combat-6/bin/bin
```

Dernier résultat : build PASS, CTest 26/26, installation PASS.

Commande de corridor depuis `workspaces/ace-combat-6` :

```bash
export TMPDIR=/fastdata/lavaulta/tmp
SDL_AUDIODRIVER=dummy xvfb-run -a \
  recompilation/ace-combat-6-demo/build-codegen-on/ac6-demo-recomp probe \
  --store .build/ac6-demo-store-test-3 --until frontend --max-ticks 3036 \
  --trace <trace> --report <report> --backend headless \
  --input-at 3000,16,0,0,0,0,0,0,1 \
  --input-at 3001,0,0,0,0,0,0,0,1
```

Pour Vulkan, remplacer le backend, créer d’abord un répertoire de captures et
définir `AC6_DEMO_AUDIT_SCREENCAP_DIR`.

## Règles de travail

- Pas d’optimisation avant affichage natif du début de mission.
- Pas d’A/B par défaut ; seulement pour une ambiguïté causale explicite.
- Aucun CPJ, worker automatique ou sous-agent.
- Inconnues en trap, aucun résultat synthétique, aucun changement du généré.
- Préserver le worktree sale : les fichiers listés dans `STATE.md` sont
  préexistants et ne doivent pas être inclus sans qualification.

## Reprise unique

La chaîne runtime/codegen démo est la seule autorisée pour le prochain gate;
elle reste séparée du projet Ghidra `ace-combat-6` divergent à `0x821B9D24`.
Le draw normal démo est entièrement couvert, résolu et écrit, mais noir car
son pixel shader bootstrap écrit RGBA zéro. L'analyse statique a montré des
initialisations distinctes de profils NSXR sans jointure vers PM4. Lire
`NEXT.md` : le prévol GDB a résolu le symbole mais n'avait pas lancé le
programme, puis son store vierge a échoué au montage VFS. Le contrat impose le
store démo complet ; l'import isolé est désormais publié sous
`/fastdata/lavaulta/tmp/ac6-nsxr-import.inm9D9/store`. L'unique fenêtre runtime
corrigée a vu les 12 lookups au tick 0, bootstrap inclus. Lire `NEXT.md` :
le thunk `0x8232710c` est sans effet et `0x822DA7F0` est une étape de
constructeur. Son appelant global `0x821A3C30` n'appelle directement aucun
producteur Xenos déjà qualifié ; cette branche ne joint donc pas NSXR au PM4.
`0x821B2BC8` consomme les champs bootstrap et les écrit dans le buffer courant.
Ses enfants `0x821B1F28` et `0x821AE1E8` n'émettent que d'autres paquets locaux.
Le chemin `0x821C5DB8` est l'initialisation bootstrap. L'autre chemin
`0x821BFBA8` construit un batch, puis `0x821BF720` pose seulement le bit
`device+0x258:0x08`; reprendre par son consommateur statique, sans runtime.
## Checkpoint le plus récent

Le champ décrit auparavant comme `device+0x258` est corrigé en `device+0x56ec` : `0x821BF720` reçoit le sous-objet `device+0x5494`. `0x821BF7D8` consomme le bit `0x08` pour configurer le drain, ne l'efface pas, et ne publie pas directement. Reprendre statiquement par `0x821A66E0`, sans runtime.
## Checkpoint le plus récent — wrapper I/O

`0x821A66E0` est un wrapper de `NtWriteFile` avec attente synchrone optionnelle. Appelé par le drain `0x821BF7D8`, il reçoit un handle du sous-objet et `0x800` octets de descripteurs. Reprendre par le producteur/ouverture de ce handle pour qualifier sa cible; ne pas appeler ceci une soumission GPU sans cette preuve.
## Checkpoint le plus récent — alias non résolu

`0x821BEFF0` initialise les canaux graphiques et `0x821BEE60` alloue leur mémoire physique, mais aucun des deux n'écrit le champ `+0x14` lu par le drain. La recherche D-form ne fournit pas de store qualifié. Reprendre par slicing statique de l'alias de ce champ; pas de runtime et pas de conclusion sur la cible de `NtWriteFile`.
## Checkpoint le plus récent — limite de slicing

Les analyses D-form et indexées existantes ne révèlent aucun producteur du champ `device+0x54a8`. Le sujet est indécidable avec le projet Ghidra actuel; ne pas le rouvrir sans un nouvel outil de slice. Reprendre par les autres appelants de `0x821A66E0` et leurs buffers, toujours statiquement.
## Checkpoint le plus récent — chaîne PM4/I-O

`0x821BFBA8` fournit la première chaîne statique buffer PM4 → `NtWriteFile`: il appelle `0x821B2BC8`, construit le buffer de canal et appelle `0x821A66E0`. Les autres appelants observés sont des I/O génériques. Reprendre par comparaison ciblée avec le point de soumission commun natif, sans runtime intégral.
## Checkpoint le plus récent — frontière de contrat

Le backend natif publie le WPTR MMIO dans `GuestBridge::apply_xenos_mmio_write`; le binaire démo utilise une enveloppe `NtWriteFile` de canal. La comparaison ne justifie aucun correctif : qualifier d'abord le layout de ce buffer pilote depuis `0x821BFBA8`, sans runtime.
# Reprise — gate contenu `0x821B96B8` fermé

Le transfert atomique de `state+0x2e28` est maintenant borné : `0x821B96B8` rend les moitiés basse/haute de la valeur pré-échange, et `0x821B9DB0` en fait une plage page-alignée dans un buffer de 11 mots. La suite utile est le producteur du buffer retourné par `0x821B9DB0` vers `0x821BA780`; ne pas reprendre la piste des handles de profils sans une nouvelle méthode statique.
# Reprise — contenu de `0x821B9DB0` requalifié

La chaîne est confirmée par les CFG démo actuels : `0x821B9AE0` réserve les dwords du contenu et `0x821BA780` transmet l’adresse et la longueur de `0x821B9DB0` à `0x821BA058`. Le prochain gate est la source des descripteurs indirects du publisher, sans rouvrir l’allocation de contenu.
# Reprise — source du descripteur indirect fermée

La source est requalifiée : `0x821BA780` fournit adresse/longueur, `0x821BA058` les empaquette en un descripteur à deux dwords, et `0x821B9BC8` le développe en indirect Type-3 dans le ring. Le prochain gate compare ce contrat aux sources et tests du bridge natif; ne pas attribuer le template de `0x821B2BC8` à ce descripteur sans nouvelle preuve de données.
## Dernier gate clos — frontière de contrat XMA

Le SDK et le CFG PAL limitent la preuve à un `HRESULT` et à l'écriture/relecture du pointeur opaque dans `entry+64`. Aucun layout de contexte, nombre d'instances, effet MMIO ou décodage n'est établi. La prochaine question est le premier consommateur PAL après la création.
## Dernier gate clos — les trois contextes XMA ne produisent pas RT0

La trace existante observe leur libération puis une frontière distincte, sans packet ni écriture RT0/EDRAM. Les détails d'allocateur local ne sont pas promus en faits PAL. Reprendre avec le consommateur statique du quatrième store XMA.
## Dernier gate clos — quatrième kick XMA

Le store est une écriture MMIO calculée par `0x82357240`, suivie d'une barrière I/O. Le périphérique XMA est son unique consommateur établi et son effet reste inconnu; ce chemin ne justifie aucune production RT0. Reprendre par les submissions PM4 ultérieures.
## Dernier gate clos — signal scheduler intègre

`E000004C` est un événement PAL créé puis signalé par les wrappers qualifiés; les 351 handoffs observés réveillent le scheduler au même tick. Reprendre à la première divergence guest neutral/START, sans changer le scheduler.
## Dernier gate clos — START est livré mais pas consommé par le menu

La chaîne brute, normalisée et logique est qualifiée; les consommateurs menu ne sont pas atteints et des boutons ultérieurs n'aident pas. Reprendre au producteur d'état post-START, sans modifier l'entrée, le scheduler ou le renderer.

## Dernier gate clos — consommateurs mission/niveau natifs

Reprendre dans `artifacts/mission-consumer-static-gate/`. Les wrappers
`sub_820EA550` et `sub_820EA598` sont implémentés et validés; la prochaine
expérience est une sonde mission bornée ou la qualification statique de leurs
getters/appelants. Ne pas conclure à la non-reachability à partir des 120 ticks
neutres.

## Dernier gate clos — getters mission/niveau et validity gate natifs

Reprendre dans `artifacts/mission-dispatch-static-gate/`. Les getters
`sub_82095B80`, `sub_820E9290` et `sub_820E9300` sont maintenant implémentés
et validés contre le projet Ghidra démo et le corps généré. La prochaine
expérience est la qualification statique de leurs appelants, avant toute
nouvelle observation runtime.

## Dernier gate clos — écrivain de validity mission natif

Reprendre dans \`artifacts/mission-validity-callers-static-gate/\`.
\`sub_821714C0\` est natif et validé; ses seuls enfants encore générés sont
\`sub_8218E088\` et \`sub_8218EA88\`. Qualifier ces deux helpers avant d'étendre
la couverture aux autres appelants de la validity gate.

## Dernier gate clos — helpers de publication de validity natifs

Reprendre dans \`artifacts/mission-helper-native-gate/\`. Les helpers de table
et de publication sont natifs et validés contre Ghidra et le code généré.
Qualifier maintenant \`sub_8216DB10\`, sans conclure sur la reachability tant
que la chaîne statique n'est pas épuisée.

## Dernier gate clos — appelant validity \`sub_8216DB10\` natif

Reprendre dans \`artifacts/mission-8216DB10-static-gate/\`. Le helper de table
et l'appelant sont natifs et validés; qualifier maintenant \`sub_8216D760\`.

## Dernier gate clos — parent validity \`sub_8216D760\` natif

Reprendre dans \`artifacts/mission-8216D760-static-gate/\`. Le parent est
natif, les deux dispatchs virtuels sont qualifiés par LR/slot exacts, et le
corps recharge la vtable avant le second appel. Build, audit, CTest (26/26) et
le symbole lié passent. Qualifier ensuite \`sub_8219EE40\` et
\`sub_8219EC88\` avant toute nouvelle sonde runtime.

## Dernier gate clos — enfants récursifs de \`sub_8216D760\` natifs

Reprendre dans \`artifacts/mission-8219EE40-EC88-static-gate/\`. Les trois
parcours de listes sont natifs, leurs appels virtuels sont qualifiés par LR et
slot exacts, et les vérifications build/audit/CTest/symboles passent. Reprendre
par les xrefs de ces trois fonctions et les producteurs des têtes de liste;
les cibles concrètes des slots 1/3 restent la prochaine frontière statique.

## Dernier gate clos — mutateur partagé des listes mission natif

Reprendre dans `artifacts/mission-list-producers-static-gate/`. Le slicing des
xrefs a isolé `sub_8219ECF8`, désormais natif avec sa free-list et ses neuf
dispatchs virtuels qualifiés. Qualifier ensuite les wrappers producteurs et les
cibles concrètes des slots 1/3; ne pas lancer de runtime avant cette réduction
statique.

## Dernier gate clos — wrappers producteurs et cibles virtuelles mission natifs

Reprendre dans `artifacts/mission-wrapper-targets-static-gate/`. Les cinq
wrappers producteurs, l'append partagé et les getters de slots 1/3 sont natifs;
les 24 couples LR/slot sont qualifiés et build/audit/CTest (26/26) passent.
Qualifier ensuite les appelants directs de ces wrappers avant toute sonde
runtime.

## Dernier gate clos — appelants directs des wrappers producteurs mission natifs

Reprendre dans `artifacts/mission-wrapper-callers-static-gate/`. Le slicing
Ghidra du projet démo canonique donne 36 xrefs et 21 appelants; les helpers
`E428`/`E768` et les six appelants courts sont maintenant natifs avec contrats
de registres, offsets et LR slot 1 exacts. Les assertions statiques, build,
audit, CTest (26/26) et installation passent. Le prochain travail est la trace
bornée SDL dummy/Xvfb vers Mission 01, avec critères de confirmation,
réfutation et indécidabilité déjà définis dans `NEXT.md`.
## Dernier gate clos — runtime borné: alias free-list avant Mission 01

Reprendre dans `artifacts/mission-runtime-reachability-gate/` avec
`producer-alias-closure.txt`. L'import qualifié s'arrête au tick 61 sur
`0x8219ECBC`/cible nulle. Les watchers hôte montrent `ac6_mission_take_node`
plaçant `0x18BB0300` dans E0 puis écrivant `0x18960180` dans le mot 0 de cet
objet. Le corps généré de `0x8219F080` confirme le contrat de l'aide native;
ne pas ajouter de garde indirecte. Qualifier ensuite le producteur du `next`
de free-list qui introduit l'adresse de tas.

## Dernier gate clos — resolver mission: sélecteur hors bornes

Reprendre dans `artifacts/mission-runtime-reachability-gate/`. La correction du
retour de `ac6_mission_take_node` a supprimé le trap du tick 61. La capture
ciblée du tick 67 établit `count=2`, `index=0x30` dans `0x821EE0F8`, retour nul,
puis lecture `r4+4` dans `0x821EDF40` au LR `0x82278184`. L'instrumentation est
retirée et la validation finale build/CTest/install est verte. Prochaine action:
qualifier statiquement le producteur du sélecteur `0x30` et des données
`0x18BD1000`; ne pas ajouter de garde au dispatcher.

## Dernier gate clos — producteur statique de l'index resolver `0x30`

Reprendre dans `artifacts/mission-runtime-reachability-gate/` avec
`resolver-index-producer-static-gate.txt` et son rapport JSON. `sub_8219E7B0`
consomme `+0x20`; `sub_821A03A8` y stocke `r8`, fourni par le retour du slot
virtuel d'offset 160 à `0x82191148` dans `sub_82191088`. Le contrat local est
qualifié, l'origine du `0x30` reste indécidable à cette frontière. Qualifier
ensuite la cible virtuelle ou documenter la limite; aucune garde resolver.
## Dernier gate clos — cible virtuelle de l’index (offset 160)

Reprendre dans `artifacts/mission-runtime-reachability-gate/` avec
`virtual-index-producer-final-static.txt` et
`virtual-index-producer-final-validation.status`. Le constructeur et la vtable
qualifient localement les slots 156/160 (`0x8223D4C0`/`0x8223D4C8`) comme
setter/getter du champ `+3164`; `sub_82191088` écrit 48 puis récupère `0x30`,
et `sub_821A03A8` le stocke dans `resolver+0x20`. La frontière virtuelle est
fermée sans garde. Le runtime reste arrêté au tick 67 sur la lecture `r4+4`;
prochaine action: qualifier le contrat du resolver et du descripteur après cet
index.
## Dernier gate clos — contrat du resolver après l’index `0x30`

Reprendre dans `artifacts/mission-runtime-reachability-gate/` avec
`resolver-contract-final-static.txt` et
`resolver-contract-final-validation.status`. Les corps PPC qualifient le
descripteur (`count +0`, `base +4`, `table +12`) et le lookup borné; le
retour nul pour `count=2,index=0x30` est intentionnel. Aucune garde ni
modification native n’est justifiée. La prochaine action est de réconcilier le
contrat de `resolver+0x20` avec ses producteurs/layouts avant toute nouvelle
sonde.

## Dernier gate clos — setter object+8 et cible virtuelle slot 16

Reprendre dans `artifacts/mission-runtime-reachability-gate/` avec
`object8-slot16-static-gate.txt`, `object8-slot16-qualified-runtime-summary.txt`
et `object8-slot16-final-validation.status`. Le setter `0x8219F594` reçoit une
cible non nulle pour l'objet `0x189600C0`, relue par `0x8219E7B0`; le trap
`0x8219E580` concerne un autre objet `0x18960100` dont `+8` est nul. Le setter
et l'aliasing sont exclus, aucune garde n'est ajoutée, et la validation finale
build/CTest/install/layout est verte. Prochaine action: qualifier le
constructeur/producteur du vtable `0x82012DC4` et du champ nul.

## Dernier gate clos — E550 et setter F42C

Reprendre dans `artifacts/object8-null-producer-gate/` avec
`vtable-rtti-summary.txt`, `setter-body.txt`, `F42C-zero-context.txt` et
`gate-decision.txt`. Le RTTI qualifie le slot 16 E550 vers `0x820D2C68`, qui
écrit `r4` à `+8`; F300 le fournit via `out`. Trois objets E550 reçoivent
pourtant zéro au tick 61, puis E580 lit ce zéro au tick 67. L'hypothèse d'un
setter absent est réfutée; aucun patch natif n'a été ajouté. Prochaine action :
tracer statiquement le producteur de `out` (`sub_8219EE40`/fallback allocator)
avant F42C.

## Dernier gate clos — cadre ABI F300 et `out`

Reprendre dans `artifacts/f300-out-producer-gate/` avec
`sub_821E1CD8-generated.txt`, `runtime-stage-summary.txt`,
`runtime-frame-outcome.txt` et `final-validation.status`. Le PPC généré impose
un cadre de 128 octets; son absence dans F300 natif écrasait le buffer `out`
pendant E1CD8. Le cadre/restauration est maintenant présent; trois passages
conservent `out` non nul jusqu'à F42C et le trap nul disparaît. Le probe atteint
100 ticks mais reste avant le jalon mission sur budget scheduler. Prochaine
action : une fenêtre runtime bornée orientée jalon mission, sans retoucher F300.

## Dernier gate clos — scheduler bloqué après F300

Reprendre dans `artifacts/mission-post-f300-gate/` avec
`outcome-summary.txt` et `scheduler-summary.txt`. La fenêtre 300 ticks confirme
l'absence de trap et la stabilité du cadre F300, mais tous les threads démarrés
sont bloqués avant mission (`runnable=0`, `blocked=23`, `finished=0`). Prochaine
action : qualifier l'événement/wait de réveil avec une trace bornée, sans
réintroduire d'instrumentation F300.

## Dernier gate clos — frontière statique des réveils d'événements

Le gate `artifacts/mission-wakeup-gate/gate.status` est fermé sans nouveau
runtime. Les contrats de wait/publication sont centralisés et les huit
événements auto-reset du mixer sont explicitement décrits comme sans producteur
dans le chemin par défaut: le client render-driver est enregistré mais son
callback audio console n'est jamais appelé. Reprendre sur la frontière
audio/XMA et le callback render-driver; ne pas ajouter de signal synthétique.

## Dernier gate clos — frontière audio/XMA du réveil mixer

Reprendre dans `artifacts/audio-xma-boundary-gate/` avec
`static-synthesis.txt`, `callback-contract-windows.txt` et
`static-oracle-check.txt`. Le callback enregistré charge `0x829DA528`, mais le
dispatch est opt-in; Register/Submit/Unregister et XMACreate/Release n'ont
aucune publication d'événement. La source absente est le pilote audio/XMA de
la console. Ne pas relancer le runtime ni injecter de signal; toute suite doit
nommer une preuve externe bornée.

## Dernier gate clos — contrat externe XAudio/XMA

Reprendre dans `artifacts/audio-xma-external-contract-gate/` avec
`official-source-check.txt`, `static-boundary-decision.txt`,
`hypothesis-criteria.txt` et `gate.status`. Les pages Microsoft Learn
disponibles ne publient pas le contrat privé render-driver/XMA; Rexglue ne
fournit qu'une corroboration secondaire du dispatch callback par worker. Aucun
producteur local des huit événements du mixer n'est identifié. Prochaine
action: preuve officielle XDK/xboxkrnl ou oracle externe borné avec le callback,
le handle et le réveil comme observables; ne pas activer le chemin expérimental
ni synthétiser d'événement.

## Dernier gate clos — producteur guest de l'état XAudio

Reprendre dans `artifacts/audio-xma-client-state-producer-gate/` avec
`static-chain-decision.txt`, `runtime-reachability-summary.txt` et
`gate.status`. La chaîne `0x8234F600 → 0x8234F058 → 0x82356410 →
0x82355F70 → 0x82356070` est exécutée au tick 106 dans les deux atlas; le
constructeur publie `0x829DA528`. Le callback `0x8236DD98`/dispatcher
`0x82355E58` n'est pas observé jusqu'au tick 253. Prochaine action : qualifier
l'activation du callback et le producteur des événements mixer, sans signal
synthétique ni mode expérimental.

## Dernier gate clos — activation bornée du callback XAudio

Reprendre dans `artifacts/audio-xma-callback-activation-gate/` avec
`runtime-summary.txt`, `hypothesis-criteria.txt` et `gate.status`. Une sonde
CPU4 explicitement expérimentale observe `0x829DA528=0x1005CEBC` et 212
entrées dans `0x8236DD98` aux ticks 62–67; elle trap au tick 67 sur l'adresse
guest `0x4` (`lr=0x82278184`). La construction et l'activation sont donc
fermées. Prochaine action : qualifier l'enfant `0x82355E58` et une éventuelle
publication vers les huit événements du mixer, sans signal synthétique ni
changement du chemin par défaut.

## Dernier gate clos — cause du trap dans l'enfant du callback XAudio

Reprendre dans `artifacts/audio-xma-callback-child-gate/` avec
`static-child-decision.txt`, `bridge-dispatch-window.txt` et `gate.status`.
`0x8236DD98` ne fait que charger le client global dans `r3`; le dispatcher
atteint `0x82278160`, puis `0x821EDF40`, dont la première lecture est `r4+4`.
La capture bornée a `r4=0` et trap `address=0x4` au tick 67 (`lr=0x82278184`).
Un `KeSetEvent` conditionnel au callsite `0x82355EA8` existe statiquement,
mais n'est pas attribué dans cette capture. Prochaine action : qualifier le
producteur du descripteur `r4` et la correspondance des événements, sans
modifier le bridge ni synthétiser de signal.

## Dernier gate clos — descripteur d'enregistrement vs `r4` enfant

Reprendre dans `artifacts/audio-xma-r4-descriptor-gate/`. Le wrapper
`0x8234D0E8` reçoit `r4=0x8236DD98` uniquement pour construire le descripteur
de registration `[callback, object+0x0c]`; ce n'est pas l'argument `r4` du
callback enfant. Le chemin `0x8236DD98 → 0x82355E58 → 0x82278160 →
0x821EDF40` laisse ce dernier non produit, et la capture voit `r4=0` au trap
`0x4` (`lr=0x82278184`). Ne pas patcher le bridge. La frontière suivante est
le contrat externe XDK/XMA du contexte enfant ou l'attribution du
`KeSetEvent` `0x82355EA8` aux handles mixer.

## Dernier gate clos — propriétaire de l'événement XAudio statique

Reprendre dans `artifacts/audio-xma-event-object-gate/`. Le constructeur
`0x82356070` initialise `0x829DA518`/`0x829DA508` et le sémaphore
`0x829DA4F4`; `0x82355818` et `0x82355E58` signalent `0x829DA518`, tandis que
`0x82355CE0` l'attend puis signale `0x829DA508`. Les références qualifiées ne
relient pas cet objet aux handles mixer, et le bridge sait déjà traiter son
layout mémoire. Ne pas patcher ni activer le chemin expérimental. La frontière
suivante est le contexte enfant privé XDK/XMA passé en `r4`.

## Dernier gate clos — contexte enfant privé XDK/XMA

L'oracle Xenia invoque le callback avec un seul argument `r3`; AC6 exige
pourtant `r4+4` dans `0x821EDF40` après le callback `0x8236DD98`, avec `r4=0`
et trap `0x4` observés. Le contexte de registration `object+0x0c` est nul et
le couple CPU4 n'est pas qualifié comme substitut. Reprendre dans
`artifacts/next-xdk-xma-r4-context-gate/`; aucune modification du bridge ni
signal synthétique. La prochaine preuve doit venir du contrat privé XDK/XMA ou
d'une capture bornée de l'argument et de la structure pointée.

## Dernier gate clos — table du slot de contexte XMA

Reprendre dans `artifacts/xma-output-slot-frontier-gate/`. `0x82356528`
construit des entrées de `0x60` octets; `entry+0x40` est testé par
`0x82357240`, passé à `XMACreateContext`, puis effacé par
`0x823567E0` au teardown. Le zero-fill `0x821A4B70` précède les écritures de
table. L'ABI privé et le MMIO `0x823572D8` restent ouverts; ne pas patcher ni
interpréter ce slot comme PCM sans preuve supplémentaire.

## Dernier gate clos — free-list XMA native

Reprendre dans `artifacts/xma-context-free-list-gate/`. Le SDK local confirme
que `XMAReleaseContext` remet un contexte opaque sur une free-list. Le bridge
réutilise maintenant un slot inactif avant d'étendre le haut-water; le test
isolé et les 27 tests complets passent. La prochaine frontière reste le
consommateur XMA privé et le store MMIO `0x823572D8`, sans modifier l'ABI ni
synthétiser un producteur audio.

## Dernier gate clos — couche privée XMA identifiée

Reprendre dans `artifacts/xma-private-mmio-gate/`. Les PDB XDK officiels
nomment `_XMA_CONTEXT_DATA`, `_XMA_REGISTERS`, le cycle de vie `CXMADecoder`
et les pointeurs `m_pHWContexts`/`m_pRegisters`, mais ne qualifient pas les
offsets ni l'effet du store `0x823572D8`. Aucun code n'a changé. La prochaine
étape est une qualification privée ciblée; ne pas activer de sémantique MMIO
ou de producteur audio par hypothèse.

Le dump `pdb-publics-official.txt` fournit des coordonnées segment:offset pour
les routines XMA et les membres privés. Reprendre par un désassemblage borné
avec ce mapping; ne pas supposer que l'offset CodeView est déjà une VA guest.

## Dernier gate clos — rôles des registres XMA indexés

Le noyau officiel confirme la formule `index>>5`/bit one-hot et les rôles
`Enable: 1A80+1940`, `Release: 1A40+1A80`, `Disable: lecture 1840 puis 1A40`.
Le scan complet du `.text` ne trouve que ces cinq stores indexés. La source
Xenia est seulement une corroboration secondaire; `XMAInitializeContext` PAL
ne contient pas le store indexé `1A80`. Le layout privé et les registres directs
restent ouverts. Reprendre dans
`artifacts/xma-private-mmio-gate/xma-kernel-register-addresses.txt` et
`gate.status`; ne pas activer le bridge expérimental.

## Dernier gate clos — layout XMA privé statique épuisé

Le SDK officiel et le TPI PDB n'exposent aucun offset de `_XMA_CONTEXT_DATA`
ou `_XMA_REGISTERS`. Xenia/ReXGlue n'apportent qu'une table générique de rang
inférieur; elle n'est pas promue en sémantique PAL. Aucun code n'a changé et
les mappings MMIO restent fail-closed.

Reprendre dans `artifacts/xma-register-layout-gate/`. La prochaine gate est une
capture bornée autour de `0x823572D8`, selon les critères H1/H2 de
`hypothesis-criteria.txt`, sans trace globale ni décodage audio.

## Dernier gate clos — capture du contexte XMA après `0x823572D8`

La variante codegen-on et les 27 tests sont verts. La capture lecture seule
`buttons16` a enregistré trois stores `0x7FEA1A80` acceptés au tick 916, pour
les indices 0..2; les trois contextes de 64 octets sont entièrement nuls.
H1 est réfutée sur ce chemin. H2 reste indécidable côté matériel faute de
consommateur post-store; aucun mapping MMIO fonctionnel n'a été activé.

Reprendre dans `artifacts/xma-runtime-state-gate/`, surtout
`capture-analysis.txt` et `capture-criteria.md`. La prochaine action est une
recherche statique du premier consommateur device-facing distinct; ne pas
répéter le trap ou la fenêtre runtime inchangée.

## Dernier gate clos — recherche statique post-store XMA

Le scan Ghidra read-only du PAL ne trouve aucune matérialisation fixe de
`1A80`/`1940`/`1A40`; les xrefs de l'aperture sont concentrées dans quatre
fonctions de cycle de vie. `0x82357240` prépare puis écrit le one-hot et
retourne sans enfant. L'effet device-facing reste indécidable.

Reprendre dans `artifacts/xma-runtime-state-gate/post-store-static-decision.txt`
et `gate.status`. La prochaine action est la chaîne ultérieure
`0x82357310`/`0x823575A8` ou une source noyau qualifiée; ne pas activer le
bridge MMIO.

## Dernier gate clos — chaîne XMA ultérieure

`0x82357458` compare l’index de table au registre direct assemblé
`0x7FEA1818..0x7FEA181B` et gère les copies/cache-clears. `0x823575A8` flushe
les blocs, recopie vers `entry+0x40`, puis écrit le one-hot `0x7FEA1940`;
`0x82357390` pose `0x7FEA1804=0x03000000` dans sa récupération. L’état guest
est distinct de `0x1A80`, mais sa sémantique hardware/PCM reste ouverte.

Reprendre dans `artifacts/xma-runtime-state-gate/later-chain-static-decision.txt`
et `gate.status`. Ne pas activer le bridge ni répéter le trap du cinquième bit;
qualifier ensuite le consommateur noyau officiel ou l’état matériel local.

## Suite — layout privé non récupéré

Les headers XDK, le TPI PDB apparié et les sources Xenia n'exposent pas le
layout `_XMA_REGISTERS`. Le consommateur ISR reste qualifié, mais ses noms de
champs et son effet device-facing sont ouverts. Reprendre dans
`artifacts/xma-kernel-consumer-gate/next-observation.md`; une capture éventuelle
doit rester bornée aux accès `0x7FEA1808`/`0x7FEA1A08` autour des trois premiers
stores `1A80`, sans répéter le trap du cinquième bit.

## Dernier gate clos — consommateur noyau ISR XMA

`CXMADecoder::InterruptServiceRoutine` est un symbole PDB officiel à `0xACE68`.
Son code lit `0x7FEA1808`, teste trois bits, écrit `0x100/0x200/0x400` vers
`0x7FEA1A08` avec barrières, puis quitte par le trampoline d'interruption.
Cette relation noyau est qualifiée, mais les champs privés et l'effet audio ne
le sont pas. Reprendre dans `artifacts/xma-kernel-consumer-gate/decision.txt`
et `gate.status`; chercher ensuite un layout officiel ou un état matériel natif
distinct, sans activer le bridge.

## Dernier gate clos — ISR native non instrumentable

Le code généré appelle seulement les imports XMA de création/libération de
contexte; il ne contient pas l'ISR ni les littéraux `0x1808`/`0x1A08`. Le bridge
ne mappe pas ces adresses, donc une lecture native serait piégée avant toute
trace. H1/H2 restent indécidables et aucun patch n'a été appliqué. Reprendre
dans `artifacts/xma-kernel-consumer-gate/native-isr-decision.md`; qualifier
ensuite l'ABI/retour des imports ou une autre frontière PAL.

## Dernier gate clos — ABI XMA create/release

Le SDK qualifie `XMACreateContext` comme `HRESULT(PXMACONTEXT*)` avec contexte
opaque; le callsite PPC teste `r3` puis consomme le pointeur écrit avant
`MmGetPhysicalAddress`. `XMAReleaseContext` est `VOID(PXMACONTEXT)`. Ce sont les
seuls imports XMA appelés par le XEX; aucun PCM ou registre privé n'est ajouté.
Reprendre dans `artifacts/xma-import-abi-gate/decision.md`; suivre ensuite le
consommateur post-create ou une autre frontière PAL.

## Gate historique supersédé — mismatch d’adresse post-création XMA

Ce bloc est conservé pour la traçabilité et ne constitue plus une hypothèse
active.

Le calcul PAL est statiquement aligné sur `0x7FEA1A80` seulement pour un index
inférieur à `0x20`; le bridge transmet les adresses sans translation, renvoie
l’identité dans `MmGetPhysicalAddress`, et son allocateur page-aligné commence
à `0x10000000`, tandis que la globale finit par `0x52C`. L’identité actuelle
ne peut donc produire le store exact (l’exemple de première allocation donne
`0x7FEA2BAC`, non mappé). La canonicalisation cachée est réfutée; ne pas
patcher la traduction avant d’avoir qualifié le producteur de l’adresse
physique attendue.

Cette lecture traitait à tort `0x829DA52C` comme une base fixe. Elle est
supersédée; ne pas patcher la traduction physique sur cette seule observation.

## Dernier gate clos — producteur statique de la base XMA

`sub_82356510` copie la lecture wire de `0x7FEA1800` dans `0x829DA52C`, puis
`sub_82357240` utilise ce slot après `MmGetPhysicalAddress` pour son index et
son `stwbrx`. Le bridge publie le même tableau et l'identité de l'adresse
physique reste cohérente; la trace bornée confirme la chaîne. Aucun code natif
n'a été modifié.

Reprendre dans `artifacts/xma-physical-producer-gate/producer-summary.md`,
`validation.txt` et `gate.status`; qualifier ensuite le consommateur privé XMA
ou une autre frontière PAL indépendante.

## Dernier gate clos — consommateur privé tardif XMA

Les quatre routines PAL tardives sont qualifiées au niveau guest : stride
96/index `+80`, copies `entry -> entry+64`, flags et one-hot indexés vers
`0x7FEA1A40`/`0x7FEA1940`, plus les accès directs `0x7FEA1804`/`0x7FEA1818`.
La validation des constantes PPC est verte. L'effet device-facing reste
indéterminé; aucun patch natif n'a été fait.

Reprendre dans `artifacts/xma-private-consumer-gate/decision.md` et
`gate.status`. La prochaine action doit qualifier un consommateur matériel
avec une observation nouvelle ou choisir une autre frontière PAL; ne pas
répéter le trap connu ni activer un mapping spéculatif.

## Dernier gate clos — formats de fetch Xenos

Les fetches atteints sont cartographiés statiquement : `57` vers
`FMT_32_32_32_FLOAT` pour la position et `38` vers
`FMT_32_32_32_32_FLOAT` pour la couleur, avec offsets/strides/registres et
exports joints. Les payloads microcode et vertex ne sont pas publiés; la sortie
PS et l'effet de rendu restent indéterminés. Aucun patch natif n'a été fait.

Reprendre dans `artifacts/shader-static-frontier-gate/decision.md` et
`gate.status`; qualifier ensuite le producteur du vertex buffer et la sortie PS.

## Dernier gate clos — producteur et adressage du vertex buffer

Le producteur natif charge `[0x127CA03C, 0x127CA0A8)` et écrit cette plage
dans le storage buffer 2 à l'offset `0x027CA03C`. Le traducteur ReXGlue, avec
quatre buffers, calcule le même binding et le même offset dword pour le fetch
position, tandis que les valeurs des trois records concordent avec l'analyse
statique atteinte. Aucun patch natif n'a été fait; la sortie PS et l'effet
EDRAM sont encore indéterminés.

Reprendre dans `artifacts/vertex-buffer-producer-gate/decision.md`,
`arithmetic-validation.txt` et `gate.status`; après rotation, qualifier
statiquement la sortie PS et le writer EDRAM.

## Dernier gate clos — sortie pixel et chaîne writer PM4

Le shader `[0x82013E80,0x82013EA4)` se limite à `max oC0,r0,r0`; ReXGlue
initialise `r0` et la couleur 0 à zéro, et la validation SPIR-V passe. Le draw
RT0 couvre les 921600 samples et résout 230400 pixels zéro. Les writers guest
du paquet RT0 (`0x821B55C0`/`0x821B5840`) puis de `RB_COPY`
(`0x821B6FD0`/`0x821B7C04`) sont qualifiés dans l'ordre. Le contenu EDRAM
guest-owned reste indéterminé.

Reprendre dans `artifacts/pixel-output-edram-gate/decision.md`,
`observations.md` et `gate.status`; chercher ensuite le premier shader pixel
non-bootstrap ou une observation bornée des samples EDRAM au consommateur.

## Dernier gate clos — frontière statique pixel non-bootstrap

Les inventaires neutre/START sont concordants : pixel atteint de 9 dwords
seulement, 24 points bootstrap puis deux rectangles partageant ce pixel. Le
corpus NSXR expose cinq pixels de 60 octets non joints et deux pixels de
36 octets joints. Aucun nouveau discriminant EDRAM n’est disponible au niveau
statique; aucun patch natif n’a été fait.

Reprendre dans `artifacts/next-nonbootstrap-gate/decision.md`,
`route-shader-census.txt` et `gate.status`. La prochaine observation doit
être une fenêtre runtime bornée sur une route post-START réellement nouvelle;
pré-enregistrer confirmation, réfutation et indécidabilité avant capture.

## Dernier gate clos — fenêtre runtime post-START

Le runner historique a été refusé avant lancement car le CLI a changé. Un
`probe` codegen-on unique, borné à 3 036 ticks avec START à 3 000 puis release à
3 001, atteint 2 928 présentations mais reste bloqué avant frontend; mission et
terminal ne sont pas atteints. La trace ne contient aucun événement graphique
ou EDRAM et le rapport ne fournit aucun payload pixel. La question non-bootstrap
est donc indécidable, sans patch et sans oracle Xenia utilisé.

Reprendre dans `artifacts/post-start-runtime-window/decision.md`,
`validation.txt`, `direct-probe-20260821-a/report-detail.txt` et `gate.status`.
La prochaine étape est une qualification statique du point d'attente guest/kernel
avant toute nouvelle fenêtre runtime.

## Dernier gate clos — identité du blocage guest

Le probe borne désormais le frontier à `0x822E559C -> 0x822F8848`, puis à
`NtSignalAndWaitForSingleObjectEx` sur la clé `0xE000004C` (LR
`0x821A69CC`), avec 23 threads bloqués. Le slot RTTI et les callsites PAL sont
qualifiés; le producteur de l'événement reste inconnu. Aucun patch, A/B ou
oracle Xenia n'a été utilisé.

Reprendre dans `artifacts/static-guest-block-gate/decision.md`,
`current-waits-compact.txt`, `event-caller-list.txt` et `gate.status`.
La prochaine étape est la jointure statique du producteur de `0xE000004C` et
de son signal `0xE0000048`.

## Dernier gate clos — producteur d'événement

La paire `0xE0000048/0xE000004C` est publiée par `sub_822EED70`;
`sub_822EEE10` appelle `NtSetEvent` sur le wait et `sub_822E4080` appelle
`NtSignalAndWaitForSingleObjectEx` avec signal/wait. La route courte montre
351 réveils, sans atteindre frontend. Aucun patch, A/B ou oracle Xenia n'a été
utilisé.

Reprendre dans `artifacts/static-event-producer-gate/decision.md`,
`producer-reports-compact.txt`, `validation.txt` et `gate.status`. La
prochaine étape est l'ownership/scheduling statique du writer au frontier
post-START.

## Dernier gate clos — ownership/scheduling du writer

Le thread 12 exécute le chemin `0x822E3EC0 -> 0x822EEE10` qui réveille
normalement `0xE000004C`, mais il est bloqué au frontier sur `0xE0000040`.
Le scheduler fiber mono-host ne peut reprendre que sur un wake de clé exacte;
23 threads sont bloqués. Aucun patch, A/B ou oracle Xenia n'a été utilisé.

Reprendre dans `artifacts/static-event-writer-ownership-gate/decision.md`,
`frontier-thread12.txt`, `generated-callchain-extract.txt`, `validation.txt`
et `gate.status`. La prochaine étape est la jointure statique du producteur de
`0xE0000040`.

## Dernier gate clos — producteur de `0xE0000040`

Le thread 2 exécute la chaîne `0x822E5660 -> 0x822E3EB8 -> 0x821A6AB0`, qui
charge le handle depuis `0x82934760`; la chaîne est observée 252 fois aux
ticks 1–252 dans les atlas neutral/start. Le callback indirect est joint au
LR `0x821C5178`, mais son enregistrement au frontier post-START reste ouvert.

Reprendre dans `artifacts/static-event-producer-0xE0000040-gate/decision.md`,
`producer-detail.txt`, `generated-event0040-candidates.txt`, `validation.txt`
et `gate.status`. La prochaine étape est la qualification statique de cet
enregistrement et du démarrage du thread 2.

## Dernier gate clos — enregistrement du callback

Le thread 1 enregistre `0x822E5660` dans le slot `+16520` via
`0x822F85B8 -> 0x822F85A8 -> 0x822E5670 -> 0x821C5D68`; le dispatcher
`0x821C5090` l'exécute sur le thread 2 à LR `0x821C5178`, aux ticks 1–252
dans les atlas neutral/start. L'objet et l'affinité restent à joindre.

Reprendre dans `artifacts/static-callback-registration-gate/decision.md`,
`registration-detail.txt`, `generated-callback-registration-extract.txt`,
`validation.txt` et `gate.status`. La prochaine étape est la jointure statique
objet/affinité; aucun runtime supplémentaire n'a été lancé.

## Dernier gate clos — objet de registration

`sub_821BB4C8` publie l’objet dans `stack+80`, puis le même pointeur est passé
à `sub_822E5670`, qui installe `0x822E5660` dans `+16520`. Le chemin de
construction ne joint pas `sub_821A5390`, et aucun appel d’affinité n’est
observé sur le thread 2. Reprendre dans
`artifacts/static-owner-object-affinity-gate/decision.md`,
`owner-affinity-detail.txt`, `affinity-wrapper-callers.txt`, `validation.txt`
et `gate.status`.

La prochaine observation, si nécessaire, est une fenêtre bornée sur le
dispatcher `0x821C5090` pour capturer l’objet, le slot de callback et
l’affinité publiée; aucune exécution Wine/Edge ou compilation n’est requise
avant ce test.

## Dernier gate clos — dispatch runtime objet/affinité

Le probe natif borné confirme 3035 dispatchs `0x821C5178 -> 0x822E5660` sur
le thread 2, avec `r31=0x10041A00` et `r11=0x822E5660`. Les 19 transitions
d'affinité observées concernent les threads 1, 12–17, 20 et 25, jamais le
thread 2. L'objet et le callback sont fermés; le CPU du thread 2 reste
indéterminé. Reprendre dans
`artifacts/bounded-dispatch-object-affinity-gate/decision.md`,
`runtime-summary.txt`, `validation.txt` et `gate.status`.

Prochaine étape : validation native locale, avec `-O3` seulement si les
résultats des contrats et de ctest restent inchangés. Aucun patch de scheduler
ou lancement Wine/Edge n'est justifié.

## Dernier gate clos — validation native O3

Les 27 tests CTest passent avec audio SDL factice. Les six contrats ciblés
passent à la fois dans la configuration existante et en `-O3`, sans différence
de statut ou de sortie. Aucun code généré ni comportement natif n'a été
modifié par la validation. Reprendre dans
`artifacts/native-validation-o3-gate/decision.md`, `results.txt`,
`o3-results.txt`, `validation.txt` et `gate.status`.

Prochaine étape : observation graphique native bornée ; créer une screencap
seulement si le runtime atteint effectivement le gameplay.

## Dernier gate clos — observation visuelle native

Le probe Vulkan natif atteint 3036 ticks, mais reste bloqué avant le frontend :
2928 notifications de présentation, zéro présentation qualifiée, zéro capture,
et aucun jalon frontend ou mission. Reprendre dans
`artifacts/visual-native-gameplay-gate/decision.md`, `runtime-summary.txt`,
`validation.txt` et `gate.status`.

Prochaine étape : fermer statiquement le blocage guest avant frontend; une
nouvelle screencap attendra un jalon visuel réel.

## Dernier gate clos — réveil natif du producteur 0xE0000040

Le producteur statique et le réveil natif sont confirmés par une sonde de trois
ticks : thread 2 publie 0040, thread 12 reprend à 821A8C88, publie 004C, puis
se remet en attente sur 0040. Aucun jalon frontend ou mission n'est atteint et
aucun patch noyau/événement n'a été appliqué.

Reprendre dans `artifacts/event0040-native-producer-gate/decision.md`,
`runtime-summary.txt`, `static-summary.txt`, `validation.txt` et
`gate.status`.

Prochaine étape : analyse statique ciblée des consommateurs 821A8C88 et
821A69CC, avec leurs arêtes de retour et écritures d'état guest.

## Dernier gate clos — boucle guest post-réveil

Le projet `ace-combat-6-demo` décompile le callback 822E3EC0 : attente 0040,
incrément, publication 004C, puis nouvelle attente 0040 tant que `state+0x18`
reste nul. Le wrapper 821A6AF0 passe timeout nul au wait, et les synchroniseurs
822E4018/822E4080/822EEE68 bouclent sur leur compteur avant SignalAndWait.
La ré-attente est donc guest, non un défaut du scheduler natif.

Reprendre dans `artifacts/postwake-static-gate/decision.md`,
`static-summary.txt`, `validation.txt` et les sorties Ghidra ciblées.

Prochaine étape : qualifier le compteur ou drapeau primaire qui doit progresser
pour quitter cette boucle avant le frontend.

## Dernier gate clos — compteur primaire et cible

Le primaire est `0x82934748` (`owner=0x82934708`, `+0x40`) avec handles
0048/004C et payload `+0x10`. `0x822E3EC0` produit les valeurs du payload via
`0x822EEE10`; `0x822E4018/0x822E4080` les comparent à `owner+0x10`.
`0x821A3CEC -> 0x822DA7F0 -> 0x822E52D0` initialise cette cible à zéro en
écrivant `last_argument-1` à `owner+0x98` avec `last_argument=1`.

Preuves : `artifacts/primary-counter-static-gate/decision.md`,
`static-summary.txt`, `validation.txt`, `gate.status`.

Reprendre par `NEXT.md` : le successeur guest de `0x822DA9C0` et les écrivains
du drapeau d'arrêt `owner+0x18`, sans nouveau runtime avant ce slice statique.

## Dernier gate clos — successeur guest et frontière service

Le slice statique suit `0x822DA9C0 -> 0x822E5540` jusqu'au contexte de rendu
`0x822F85B8`, avec une branche d'erreur sur le retour de `0x821BB4C8`. Le
service de boucle `0x8238CDA0` est résolu par RTTI jusqu'à `0x820FF988` puis
`0x820FF8D8`; ce dernier dépend du drapeau BSS `0x826E2374`. Les pointeurs
`0x82822F08` et `0x828819A4` restent BSS.

Reprendre dans `artifacts/frontend-successor-static-gate/decision.md`,
`static-summary.txt`, `validation.txt`, `gate.status` et `marker-scan.txt`.

Prochaine étape : qualifier les écrivains des trois champs BSS avant toute
capture runtime.

## Dernier gate clos — écrivains BSS et services statiques

Les trois champs BSS sont maintenant attribués : worker `0x826E2374` écrit par
`0x820FFCA0`; gestionnaire `0x82822F08` construit par `0x82259FF8` avec RTTI
`CAce6TaskManager@ACE6`; callback `0x828819A4` initialisé vers l'objet statique
`0x823C0D90` de type `CLayeredDrawCallBack`. Le mapping corrigé de la vtable du
gestionnaire est `+0x04 -> 0x82259D10`, `+0x08 -> 0x82259DA8`, `+0x0C ->
0x82259E18`, `+0x10 -> 0x82259E90`, `+0x18 -> 0x82259F58`.

Les consommateurs qualifiés restent la file de tâches et les couches de draw;
aucun jalon frontend n'est fermé. Reprendre dans
`artifacts/bss-service-static-gate/decision.md`, `static-summary.txt`,
`validation.txt`, `gate.status`, `correct-service-slots.raw.txt` et
`marker-scan.txt`.

Prochaine étape : qualifier statiquement le premier consommateur frontend
après les services task/layered; aucun runtime supplémentaire avant ce slice.

## Dernier gate clos — premier consommateur frontend

La fabrique de transition `0x82191468` construit un `CModeTaskTitle`
(`vtable=0x8200F01C`) puis `0x82190B18` l'insère dans la file du gestionnaire
par `0x82259E18`. `0x82259D10` parcourt cette file; les méthodes de titre
utilisées sont le poll PPC `0x8216C940` (`+0x28`) et le traitement
`0x8218A7A8` (`+0x10`). La jonction service -> objet frontend est donc
statique et qualifiée.

Reprendre dans `artifacts/frontend-consumer-static-gate/decision.md`,
`static-summary.txt`, `validation.txt`, `gate.status`, `marker-scan.txt` et
les sorties Ghidra ciblées. La prochaine étape est la provenance de la source
compacte du clip titre; la sélection runtime de la fabrique n’est pas encore
mesurée.

## Dernier gate clos — provenance de la source compacte du clip

La chaîne `0x82278F78 <- 0x8227A898 <- 0x8227AAC0` et ses appelants
`0x82127D40`, `0x8219A060`, `0x821A1170` sont qualifiés dans le démo PAL. Ils
alimentent un ordonnanceur générique via `0x821EE0F8`, `0x821A6808` et
`0x821A6168`, sans relation statique avec un constructeur ou une ressource du
clip titre. La provenance de l’entrée compacte est donc indécidable par le
CFG disponible; aucun runtime supplémentaire n’est requis à ce point.

Reprendre par `NEXT.md` : décoder isolément la fenêtre de flux titre
post-START, sans relancer le programme.

## Dernier gate clos — parser local du flux titre

Le parser fondé sur `0x823246C0` explique 23 appels de boîte et 13 événements
`opcode=7`; il atteint le premier mot hors vocabulaire à
`0x2DCB2448 = 0x1A`. Une ligne agrégée plus ancienne donne `0x243C = 0x15`,
mais la séquence détaillée la contredit et cette anomalie est documentée.

Reprendre par `NEXT.md` : producteurs statiques du triplet `mode / mission /
level` lu par le film de titre. Le runtime reste inutile à cette frontière.

## Dernier gate clos — absence de producteur natif après START

Le slice statique mission/niveau est épuisé : `0x82095B80` lit `+0x6C4`,
`0x820E9290` lit `+0x6D0`, et le seul store entier direct de mission est
`0x82171988`; aucun store entier de niveau n’est qualifié. La capture
codegen-on-b injecte START au tick 3000 et observe 3000–3004 : index 0,
mission `2048` à `0x823C2F14`, niveau `4096` à `0x823C2F20`, sans store vers
ces champs ni vers `base+0x78`. Le probe atteint 3005 ticks sans jalon.

Reprendre par `NEXT.md` et
`artifacts/title-film-state-producers-gate/decision.md` : la prochaine
étape est l’attribution statique du producteur bytecode/VM autour de
`0x2DCB2448=0x1A`.

## Dernier gate clos — attribution bytecode/VM du film

`0x823246C0` est un interpréteur de statements : PC dans le buffer, opcodes
`0..7`, dispatchs `ASContext`; `0x82278F78` est le chargeur bitstream du même
buffer. Le corps VM ne touche aucun offset du triplet `+0x6C4`, `+0x6D0`,
`+0x78` et n’appelle aucun getter/setter d’état. Les requêtes restent natives
(`0x820EA550`, `0x820EA598`, `0x82171988`).

Le gate est fermé par attribution positive de la VM comme consommateur, sans
la désigner producteur. Détails : `artifacts/title-bytecode-vm-static-gate/`.
Reprendre par `NEXT.md` : écrivains natifs hors fenêtre START, surtout
`base + index * 0xAAB8 + 0x6D0`.

## Dernier gate clos — écrivains explicites du niveau

Les cinq stores PPC directs `+0x6D0` sont des stores flottants d’une famille
de corps qui utilisent des bases globales distinctes, sans singleton titre ni
stride de slot. Les trois pointeurs `+0x6D0` passés à `0x821F1500` servent à
la lecture de métadonnées : ce callee n’écrit pas à travers le pointeur.

Le champ niveau du slot titre n’a donc aucun écrivain explicite qualifié.
Reprendre par `NEXT.md` et `artifacts/title-level-writer-static-gate/` :
remonter l’initialisation groupée du singleton `0x823C27E0` / `+0x70`.
## Dernier checkpoint

Le gate `title-swg-vtable-slot-callsite-static-gate` est limité. La passe globale sur `+0x0C` a trouvé 289 dispatches, sans lien statique avec l'instance cible. La suite est définie dans `NEXT.md`; aucun oracle n'a été exécuté.
## Dernier checkpoint

Le gate `title-swg-context-producer-static-gate` est limité. Le contexte passé à `FUN_82324188` est exactement `param_2` de `Function_820D29E0`; son producteur est au-delà d'un dispatch virtuel sans appelant direct. La prochaine piste statique est définie dans `NEXT.md`.
## Dernier checkpoint

Le gate `guest-wait-frontier-static-gate` écarte XMA comme cause immédiate : six kicks contextualisés ne réveillent pas les 23 threads bloqués et ne produisent aucun frontend. La prochaine piste est le writer EDRAM/source `RB_COPY`, définie dans `NEXT.md`.
## Dernier checkpoint

Le gate `rb-copy-surface-static-gate` est fermé : RT0 → `RB_COPY` → readback Vulkan → EDRAM est relié statiquement. Le contenu reste noir ; la prochaine frontière est le premier writer réel du draw RT0, détaillée dans `NEXT.md`.
## Dernier checkpoint

Le gate `rt0-writer-inputs-static-gate` est fermé : `normal_draw_command_` n'est jamais renseigné, donc le draw Vulkan RT0 ne peut pas s'exécuter. Le prochain correctif minimal et son test sont définis dans `NEXT.md`.

## Dernier checkpoint

Le gate `ib-bootstrap-writer-runtime-gate` est limité après cinq lots. Un
build guest qualifié exécute un tick, sans écriture dans
`[0x16AE0980, 0x16AE0A40)` mais aussi sans soumission d'IB observable : le
résultat est indécidable. Reprendre par `NEXT.md` et
`artifacts/ib-bootstrap-writer-runtime-gate/BLOCKER.md`; la prochaine étape
est l'observable minimal au point de première soumission.

## Dernier checkpoint

Le gate `ib-first-submit-observable-gate` est limité après cinq lots. Le hook
existant capture l'IB `0x16AE0980` au tick 0 sur deux builds, sans store dans
la fenêtre. La zone n'est pas mappée avant l'entrée guest. Le gate suivant a
depuis établi que les stores générés passent déjà par le watcher configurable.

## Dernier gate clos

`generated-store-window-gate` réfute la modification proposée : les cinq
adaptateurs de stores générés, y compris VMX, passent déjà par
`GuestMemory::store_*` et son watcher configurable. Aucun code n'a été changé.
Reprendre par `NEXT.md` et
`artifacts/generated-store-window-gate/decision.md` : qualifier les 48 dwords
courants et le mapping de `0x16AE0980` dans un seul tick.

## Dernier checkpoint

`ib-payload-mapping-gate` est limité après cinq lots. GDB qualifie
`map_zero(0x16ADF000, 8192)` avant la capture de `0x16AE0980` au tick 0, mais
ne peut pas lire les retours optimisés de `load_u32`. Reprendre par `NEXT.md`
et `artifacts/ib-payload-mapping-gate/BLOCKER.md` : imprimer uniquement les 48
dwords dans `trace_ib_capture`, avec un test synthétique.


## Dernier gate clos

`ib-payload-trace-gate` qualifie le payload de `0x16AE0980` : 48 mots,
exactement 24 paquets `PM4_DRAW_INDX_2` PointList avec initiateur
`0x00010081`. L’attente littérale historique `0x2D` est réfutée. Reprendre
par `NEXT.md` et le rapport/trace d’un tick déjà présents afin de trouver la
première frontière post-IB, sans nouvelle exécution par défaut.
# Reprise — chaîne de réveil `0xE0000040`

Le producteur n'est plus inconnu : thread 2 exécute
`0x822E5660 → 0x822E3EB8 → 0x821A6AB0`, chargeant `0xE0000040` depuis
`0x82934760`. Reprendre par le gate statique de registration/scheduling au site
indirect `LR=0x821C5178`; ne pas ajouter de signal synthétique avant d'avoir
qualifié pourquoi ce callback cesse d'être exécuté.

## Dernier gate clos

`framebuffer-5800-classification-gate` établit qu'il n'existe pas encore de
framebuffer rendu : les 5 692 notifications sont des appels `VdSwap`, tandis
que le ring compte zéro soumission et zéro commande `present`. Reprendre par
la chaîne statique producteur/consommateur du paquet de swap jusqu'à
`RuntimeRendererFrontier::consume`.

## Dernier gate clos

`present-command-producer-static-gate` localise la rupture : le buffer système
contenant `XE_SWAP` est construit mais jamais publié au processeur Xenos. Le
parseur et le renderer sont déjà qualifiés. Reprendre par un test synthétique
du point partagé de soumission, puis seulement par un probe borné.

## Gate actif limité

Le raccord borné de `VdSwap` est implémenté et son test ciblé passe. Le probe
de 300 ticks n'a pas livré de résumé exploitable au cinquième lot. Reprendre
strictement par les fichiers listés dans
`artifacts/vdswap-bounded-submit-implementation-gate/BLOCKER.md`; ne pas
relancer avant d'avoir classé l'échec existant.

## Rotation — hypothèse `VdSwap` réfutée

Le rapport existant prouve que la soumission directe multiplie artificiellement
les `present`; elle a été retirée et le build/test ciblé repassent. Reprendre
par les artefacts statiques existants du writer WPTR, en priorité la fenêtre
`0x821B9BC8`, pour retrouver la publication PM4 réelle.

## Dernier gate clos

`wptr-821b9bc8-static-gate` ferme la piste du compteur : `0x827AD2F0` n'est
pas un gate de rendu et `0x821B9BC8` n'est que le writer WPTR du bootstrap.
Reprendre avec `verify_ac6_pal_d3d_callback_bridge.py` sur l'image PAL démo
qualifiée pour suivre la publication asynchrone de `KickOff`.

## Gate actif limité

Le vérificateur trouve toutes les ancres et routes D3D dans l'image courante,
mais sa garde d'identité interne refuse cette révision. Reprendre par la
provenance directe décrite dans
`artifacts/d3d-kickoff-callback-static-gate/BLOCKER.md`, sans calcul ni
comparaison d'empreinte.

## Reprise après fermeture de provenance callback D3D

Le gate de provenance est fermé dans
`artifacts/d3d-callback-image-provenance-gate/decision.md`. La garde interne du
vérificateur est obsolète ; ses 33 ancres, 8 fonctions et 5 producteurs sont
qualifiés pour l'image PAL démo courante.

Réutiliser les exports existants pour suivre `0x821BA780`, `0x821BA1F8`,
`0x821BAA78`, `0x821C4A60`, `0x821C5190` et `0x821B9710`, puis produire un
rapport compact sur le dernier effet observable. Pas de runtime, Wine ou shim
avant fermeture statique.

## Rotation — reprise sur la jointure ring/WPTR

Le gate callback est fermé dans
`artifacts/post-kickoff-producer-static-gate/decision.md` : les callbacks et
interruptions sont en aval du kick et ne publient pas le ring.

Reprendre statiquement avec les producteurs/consommateurs des champs manipulés
par `0x821C57D0`, puis joindre ce curseur au writer WPTR `0x821B9BC8` ou à un
writer alternatif. Exclure `device+21508` et `0x827AD2F0` (compteurs de
performance), ainsi que toute soumission directe depuis `VdSwap`.

## Reprise — payload nul de la file guest

Le gate `ring-cursor-to-wptr-static-gate` est fermé négativement dans
`artifacts/ring-cursor-to-wptr-static-gate/decision.md`. `0x821C57D0` ne produit
pas un nouveau ring primaire, et le consommateur de file guest tourne bien.

Reprendre avec les xrefs/writers des slots `0x82386DD0` et `0x82386D90`, la
fonction productrice `0x820FF710` et ses appelants. L'objectif est d'identifier
la source de payload laissée nulle, sans injection, runtime ou fallback visuel.

## Gate payload limité

`0x820FF710` est décompilé : zéro littéral vers `slot[index]+64`, puis avance
de l'index. `artifacts/render-queue-payload-source-static-gate/BLOCKER.md`
définit le test restant : slice P-code borné des writers du champ dynamique.
Réutiliser la procédure Ghidra existante avant d'ajouter un script.

## Gate de déplacement immédiat limité

`FindPpcMemoryDisplacement.java` ne retrouve même pas le témoin
`0x820FF710` avec `0x110` : l'adresse est construite par registres. Voir
`artifacts/render-queue-slot-writer-slice-gate/BLOCKER.md`.

Reprendre dans un nouveau gate avec une passe minimale sur le P-code haut,
qualifiée d'abord par la redécouverte de `0x820FF710`. Pas de runtime, Wine,
shim ou injection de payload.

## Passe P-code créée, exécution longue à reprendre

`scripts/FindScaledStoreWriters.java` existe et impose le témoin. Premier run
sans résultat persistant avant terminaison autour de 30 secondes. Voir
`artifacts/render-queue-slot-pcode-normalization-gate/BLOCKER.md`.

Ajouter uniquement des `flush`/compteurs, puis attendre la fin du processus
Ghidra. Ne pas changer le filtre avant de connaître le résultat du témoin.

## Famille de payload fermée

Le scan a terminé : `0x820FF710` écrit type `0`; `0x820FF788`, `0x820FF7F8`,
`0x820FFA88`, `0x820FFB50` écrivent types `1..4`. Voir
`artifacts/render-queue-slot-pcode-normalization-gate/decision.md`.

Reprendre par les références entrantes de `0x82117410`. Cette fonction appelle
types `1` et `4` sans garde locale ; si elle était atteinte, les slots ne
resteraient pas nuls. Aucun runtime avant épuisement de ses appelants statiques.

## Appelant unique, garde à slicer

`0x82117410` a un seul appelant : `0x8210A1C0`, site `0x8210ADB4`. Voir
`artifacts/render-queue-writer1-reachability-gate/BLOCKER.md`.

Ne pas déduire la garde depuis les accolades du C décompilé. Reprendre avec
CFG/post-dominateurs, puis propager la condition P-code vers son champ mémoire.

## CFG du producteur exporté

`artifacts/render-queue-callsite-control-slice-gate/control-slice.txt` recense
18 choix nécessaires. `0x8210A1C0` a un appelant unique :
`0x82165CC0:0x82165D8C`.

Reprendre depuis la décompilation déjà présente de `0x82165CC0`, extraire les
arguments et leurs champs source, puis les joindre aux conditions. Aucun nouvel
export global ni runtime avant cette jointure.

## Reprise — render-queue-type1-payload-layout

Gate précédent fermé: type bas 16 bits `1` sélectionne route `0x82117410`; cinq couples garde lookup/champ `+0x10C` doivent tous passer. Reprendre depuis `artifacts/render-queue-producer-arguments-gate/callee-8210a1c0.c.txt`, lignes 550–688. Extraire offsets exacts de payload, bornes et booléens, puis chercher producteurs statiques du premier champ discriminant. Ne pas lancer runtime ni ajouter shim.

## Reprise — resource-10c-producer-slice

Trois gates statiques fermés dans rotation; reprendre nouvelle session. Payload type 1: cinq chaînes length16+bytes, puis 3+2 drapeaux optionnels. Producteur non nul de ressource `+0x10C`: `0x8231BCB0`; remise à zéro secondaire: `0x820CBAB0`. Partir de `artifacts/render-queue-type1-payload-layout-gate/resource-10c-writer-context.txt` et décompiler slice minimal autour du store dans `0x8231BCB0`. Aucun runtime, aucun shim avant preuve.

## Reprise — record-10c-store-enumeration

Ne pas reprendre `0x8231BCB0`: faux producteur render, parseur RIFF/WAVE global. Gate précédent limité après cinq batches. Trouver procédure Ghidra qualifiée déjà utilisée dans workspace, puis lancer `FindPpcMemoryDisplacement.java` avec ses trois arguments: début mappé, fin exclusive, déplacement `0x010C`. Filtrer stores, puis slicer base jusqu’aux records de `DAT_826F6124`. Aucun runtime ni shim.

## Reprise — resource-record-alias

Launcher Ghidra qualifié: `.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless`. Projet `ace-combat-6-demo`, programme `Default.xex`. Inventaire exact `+0x010C`: 101 stores. Candidat fort `0x8210D950`; ses consommateurs `0x8210D9C0/DA80` libèrent handle. Reprendre par décompilation minimale de `0x8210D8A0`, puis joindre son retour à table `DAT_826F6124` et au lookup `0x821080D0`. Ne pas runtime ni coder avant alias prouvé.

## Reprise — native-resource-registration-contract

`0x82327100` est helper de sauvegarde, pas getter. Chemin qualifié partage `DAT_826F6124`. Couverture objet prouvée pour quatre racines; parcours sauvegardé dans `artifacts/native-resource-registration-contract-resume-gate/callgraph-edges.json`. Gate limité car mapping import vaut zéro malgré 238 entrées: qualifier clés réelles d’une entrée, reconstruire mapping, trouver chemin minimal vers premier import. Aucun runtime.

## Reprise — resource-indirect-dispatch

Gate imports fermé: jointure exacte contre 228 thunks, zéro hit dans les
closures de 67/61/3/175 fonctions. Ne plus chercher un import direct. Partir
des désassemblages sauvegardés, inventorier les appels indirects atteignables
depuis `0x82108918` et `0x8210DD70`, puis relier le premier à l'objet et au slot
de vtable. Aucun runtime avant cible statiquement indécidable.

Gate indirect limité. `0x821075A0` est à profondeur 1 depuis `0x8210A1C0` et
contient trois appels indirects, avec LR `0x821075E4`, `0x821075FC`,
`0x82107614`. Le lowering passe le champ contexte `+0x108`; aucune LR n'est
dans la table de slots qualifiée, mais le helper résout encore génériquement.
Décompiler seulement cette fonction dans Ghidra et slicer les trois `bctrl`
pour obtenir objet et offsets de slots. Aucun runtime.

Slice Ghidra obtenu dans le projet démo: `0x821075A0` utilise toujours
`PTR_PTR_82386C10`; deux appels au slot 1 (`+0x04`) puis un appel au slot 13
(`+0x34`). Reprendre par xrefs écriture/lecture de `0x82386C10`, retrouver les
vtables possibles et comparer uniquement leurs slots 1/13 au manifeste. Aucun
runtime.
## Reprise — ABI PPC du lookup des receivers slots 20/21

`0x82220670` retourne une entrée de tableau ou son alias `+0x18C`; le résultat
alimente les slots 20/21. Le précédent export Ghidra a échoué avant extraction
car `Ac6XenonWords.java` attend des hexadécimaux sans `0x`. Reprendre avec
`8210A1C0 82220670 821154C0 82115530`, puis résoudre les vtables possibles.
Aucun runtime ni shim.
## Reprise — producteurs des types du lookup slots 20/21

ABI fermée : `r3 = *( *(0x823C27E0)+0x25F68 )+0x308`, `r4 = ID payload`
éventuellement remappé; `0x82220670` retourne entrée ou alias `+0x18C`, puis
les consommateurs appellent les slots `+0x50/+0x54`. Borner maintenant les
types par les stores vers le tableau et vers `+0x18C`, puis comparer leurs
cibles de vtable au codegen. Aucun runtime ni shim.
## Reprise — xrefs du setter d'alias slots 20/21

Routes prouvées : stores directs de `0x8210A1C0` depuis les objets ressource
`record+0x10C`, et setter `0x82220550` depuis le sélecteur `0x821E1D80`.
Les callgraphs sauvegardés n'ont aucun appelant du setter. Extraire ses xrefs
directs dans Ghidra, slicer les appelants et relier les valeurs à leurs
constructeurs/vtables. Aucun runtime ni shim.
## Reprise — arguments du sélecteur d'alias slots 20/21

Xref fermé : `0x82216498` est l'unique appelant direct de `0x82220550`; il
passe le conteneur global `+0x308` et un indice actif normalisé. Le setter
filtre `entrée+0x170`, puis écrit le retour de `0x821E1D80` en `+0x18C`.
Exporter/slicer le PPC de `0x82220550`, `0x821E1D80`, `0x82220750` pour borner
les tables et vtables possibles. Aucun runtime ni shim.

Gate `resource-slot20-alias-selector-args-gate` limité après cinq batches. Les
arguments et le lookup à deux niveaux sont fermés, mais la recherche scalaire
est trop large. Reprendre dans une nouvelle branche avec le test exact décrit
dans `artifacts/resource-slot20-alias-selector-args-gate/BLOCKER.md` : fonctions
référençant `PTR_DAT_823C27E0`, construction de `0x29698`, puis `lwzx/stwx`.
Aucun runtime, Wine ou shim.

Gate `resource-slot20-indexed-table-access-gate` limité après cinq batches.
`ClassifyPpcOffsetUses` trouve 120 sites/114 fonctions et contrôle positivement
`0x82220640`, mais la valeur seule est trop commune. Reprendre dans une nouvelle
branche par propagation locale du registre chargé depuis `0x823C27E0`, puis
intersection avec les accès `+0x29698`. Voir
`artifacts/resource-slot20-indexed-table-access-gate/BLOCKER.md`. Aucun runtime,
Wine ou shim.
## 2026-08-21 — slots receiver qualifiés

La factory `0x82093658` conserve la vtable receiver `0x82000B94`. Ses slots
20/21 sont `0x8220E428` (retour `0`) et `0x82211040` (getter/différence des
floats `+0x70/+0x74`), tous deux absents textuellement du natif. Reprendre par
la configuration de couverture codegen, avec
`artifacts/resource-slot20-container-factory-vtable-gate/RESULT.md`. Aucun
runtime, Wine, shim ou optimisation.
## 2026-08-21 — reprise par manifeste codegen

Le gate de vtable est fermé, mais la couverture générateur ne l'est pas. Le
C++ jeu est build-only et aucune adresse littérale `0x8220E428/0x82211040`
n'est présente dans le checkout; cela ne suffit pas si le manifeste emploie
des identifiants non littéraux. Reprendre avec le test exact décrit dans
`artifacts/resource-slot20-codegen-coverage-gate/BLOCKER.md`. Aucun code n'a
été modifié; aucun runtime, Wine, shim ou `-O3`.
## 2026-08-21 — slots 20/21 déjà générés

Le manifeste effectif et `ppc_func_mapping.cpp` prouvent que `0x8220E428` et
`0x82211040` sont présents; le C++ généré contient leurs callsites. Ne pas
ajouter de stub et ne plus traiter cette branche comme un trou codegen. Voir
`artifacts/resource-slot20-codegen-manifest-gate/RESULT.md`. Reprendre au
premier PC/import réellement bloquant du handoff courant.
## 2026-08-21 — reprendre après le load64 cycle 1761

La piste service/event manquant n'est plus prioritaire: `E000004C` atteint
déjà l'activation scheduler 351/351. La frontière autoritative est le load64
unique à `0x82327154`, suivi de traces divergentes mais sans divergence
graphics/scheduler qualifiée. Reprendre uniquement par l'alignement des deux
traces existantes décrit dans
`artifacts/first-real-native-boundary-gate/BLOCKER.md`.
## 2026-08-21 — capsule cycle 1761 épuisée

L'alignement exhaustif des 22 153 événements par route trouve seulement deux
différences `input`, dont `buttons=0/16` au tick 252; aucune divergence guest,
graphics, scheduler ou readback n'est enregistrée. Ne plus chercher un tuple
non-input dans cette capsule. Reprendre par les slices START statiques
1630/1633/1634/1635 afin de définir une fenêtre de capture minimale.
## 2026-08-21 — ne pas recapturer tick268

Une capture bornée existante contient déjà le premier différentiel utile:
START-only `0x820CDC20 → [0x2E3D3C0C] = 0`, contre une séquence neutre à
`0x2E3D44F0` via `0x823255F0/0x82325644`. Reprendre par le slice statique des
trois PC selon
`artifacts/start-minimal-observation-window-gate/RESULT.md`; aucun runtime
avant classification.
## 2026-08-21 — tick268 fermé, ne pas corriger le shim

Les trois valeurs précédemment appelées `pc` sont des LR d'appel. Les deltas
`0x2E3D3C0C/0x2E3D44F0` sont du bruit de pile/cadre d'appel; aucun état guest
persistant, task enqueue ou consommateur graphique n'est démontré. Reprendre
par la sélection statique du premier store START persistant hors pile décrite
dans `NEXT.md`. Aucun runtime avant cette sélection.
## 2026-08-21 — reprise par les lecteurs de 0x82798488

Le gate n'a trouvé aucun store START persistant qualifiable: les trois globals
connus sont transitoires et la capture tick268 vise une autre plage mémoire.
Reprendre exclusivement par les xrefs Ghidra directs/TOC/propagés de
`0x82798488`, puis slicer chaque branche dépendante jusqu'au premier store
durable. Voir
`artifacts/start-first-persistent-store-static-gate/BLOCKER.md`. Aucun
runtime, Wine ou shim.
## 2026-08-21 — reprendre en amont des propriétaires START

La chaîne statique est fermée jusqu'au premier effet durable:
`0x821DE6F8 → 0x82798488 → 0x82170FCC → 0x82171128 →
[0x827435F8]+0x18`. Le propriétaire `0x82170F58` et le second propriétaire
`0x82185198` ne sont jamais atteints. Reprendre par leurs
constructeurs/factories/vtables et leur enqueue vers `0x82259D10`. Aucun
runtime, Wine ou shim avant classification du prédicat bloquant.
## 2026-08-21 — reprendre au retrait de CTaskLoading

Les tâches consommatrices de START sont hors phase. Le dispatcher
`0x82259D10` conserve startup-demo, loading et mode-manager; `CTaskLoading`
atteint l'état interne 1 sans sortie dans `0x8217E3E0`. Reprendre par le
retour de son slot 4 `0x8218CE20` et les mutations
`0x82259E18/0x82259FF8`. Aucun runtime, Wine ou shim avant résolution du
contrat de retrait.
## 2026-08-21 — reprendre depuis le listing dispatcher existant

Ne pas relancer Ghidra. Le listing PPC des cinq fonctions est déjà dans
`artifacts/loading-task-retirement-contract-gate/ghidra-function-listings.txt`.
La branche état 1 est sans transition interne; l'état 2 seul peut publier la
requête globale. Extraire maintenant les fenêtres thunk/dispatcher/liste pour
qualifier la consommation du retour et le prédicat de retrait. Aucun runtime,
Wine ou shim.
## 2026-08-21 — reprendre par les writers de CTaskLoading+0x0C

Le CFG dispatcher est fermé: le retour du slot 4 est ignoré,
`0x82259E18` insère et `0x82259FF8` initialise. Aucun retrait automatique
n'existe dans ce chemin. Reprendre statiquement par tous les writers de
`CTaskLoading+0x0C` et leurs callbacks afin d'identifier le producteur
externe attendu par l'état 1. Aucun runtime, Wine ou shim.

Gate `resource-slot20-global-base-join-gate` limité. Faits solides :
`0x821E0B7C` initialise `[object+0x29698]` avec `object+0x2969C`;
`0x82212E54` écrit le champ global depuis `param1+0x2E4/+0x2E8`. Ignorer
`global-base-classification.txt`, invalide. Reprendre par propagation minimale
sur les basic blocks Ghidra comme décrit dans le blocker. Aucun runtime, Wine
ou shim.

Gate `resource-slot20-global-base-cfg-gate` limité. Le nouveau script SSA
compile mais s'arrête sur `Long.decode("821e0a70")`; aucun résultat binaire n'a
été produit. Corriger uniquement ce parseur en hexadécimal à préfixe optionnel,
puis relancer la même commande. Voir le blocker. Aucun runtime, Wine ou shim.

Gate `resource-slot20-global-base-ssa-retry-gate` limité. Le parseur est corrigé
et les témoins SSA passent, mais le scan libre sur-approxime à 1119 opérations.
Reprendre en passant les 120 PC `use=` au script et en filtrant par `Seqnum`
avant inspection. Voir le blocker. Aucun runtime, Wine ou shim.
## 2026-08-21 — reprise après allowlist SSA limitée

Le classifieur par PC a supprimé le surcomptage: 117/120 qualifiés, deux
rejetés, seul `0x82210188` est manquant parce qu'il n'appartient à aucune
fonction Ghidra. Reprendre avec `DumpRange.java` sur une fenêtre bornée autour
de ce PC et slicer la base du `lwzx`. Voir
`artifacts/resource-slot20-global-base-ssa-allowlist-gate/BLOCKER.md`. Aucun
runtime, Wine ou shim.
## 2026-08-21 — gate orphelin fermé

`0x82210188` est un load qualifié de `*( *(0x823C27E0)+0x29698 )`; chaîne PPC
exacte dans `artifacts/resource-slot20-orphan-82210188-gate/RESULT.md`.
Inventaire final: 118 qualifiés, deux rejetés, zéro inconnu. Reprendre la
résolution statique des vtables des objets retournés par `0x821E1D80`, pour
les slots `+0x50/+0x54`. Aucun runtime, Wine ou shim.
## 2026-08-21 — producteurs de table encore en amont

`0x82212DF0` a 15 appels directs dans 9 fonctions; tous sélectionnent
`object+0x2E4` ou `object+0x2E8`. Les callsites sont consommateurs, pas
producteurs. Reprendre par un recensement PPC exact des stores vers ces deux
déplacements, puis slicer leurs valeurs. Voir
`artifacts/resource-slot20-table-object-producer-gate/BLOCKER.md`. Aucun
runtime, Wine ou shim.
## 2026-08-21 — receiver localisé dans le populator

Les vrais producteurs de `object+0x2E4/+0x2E8` sont trois paires de conteneurs
embarqués, toutes peuplées par `0x820A4F58`. Cette routine écrit le tableau à
`entry+0xD8` et la borne à `+0xDC`; les receivers sont ses objets `piVar16`.
Reprendre depuis
`artifacts/resource-slot20-object-field-stores-gate/container-populator.log`
et extraire les définitions dominantes de `piVar16`, puis leur vtable. Aucun
runtime, Wine ou shim.
## 2026-08-21 — vtable du conteneur qualifiée

Les receivers `piVar16` sont créés par le slot `+0x14` de la vtable commune
`0x82000B94`; les six conteneurs convergent statiquement vers cette table.
Reprendre par lecture directe de `0x82000BA8`, décompiler la factory résolue,
puis relever les vtables receivers et leurs slots `+0x50/+0x54`. Voir
`artifacts/resource-slot20-pivar16-producer-gate/BLOCKER.md`. Aucun runtime,
Wine ou shim.

## Resume: CTaskLoading field-transition contracts

Le gate d'identité est fermé. Ne plus attribuer `0x8217E3E0` directement à
`CTaskLoading`. La classe observée utilise la vtable `0x8200F388`, construite
par `0x8218BF18`; son slot 4 est le thunk `0x8218CE20` vers `0x8218CCD0` quand
`this+0x0A==0`.

Reprendre statiquement sur les listings complets sous
`artifacts/loading-vtable-identity-gate/`, puis reconstruire les contrats de
`0x8218CBD8`, `0x8218CCD0`, `0x8218CD78` et de leurs trois appelants directs.
Ne pas utiliser de shim env, Wine ou oracle Xenia pour ce gate.

## Resume: CTaskLoading async-status provider

Le gate des writers est fermé par réfutation : `CTaskLoading+0x0C` est modifié
uniquement par les méthodes internes bornées. `0x8218CBD8` arme l'état 1;
`0x8218CCD0` poll `0x8219F5D0` et ne revient à l'état 0 que sur statut non nul.

Reprendre statiquement par le type de ressource de `0x8219EE40`, le contrat de
`0x8219F5D0` et leur correspondance dans les services natifs. Artefacts source :
`artifacts/loading-field-transition-contracts-gate/`. Aucun shim env, Wine ou
oracle Xenia pour ce gate.

## Resume: native loading-contract integration

Trois gates sont fermés dans cette rotation. Le provider guest est maintenant
qualifié : `0x8219F5D0` draine une file et poll le slot `+0x14` des travaux avec
le contrat `0=pending`, `-1=error`, autre=`done`. Ce contrat n'existe pas dans
les sources ni le build de `ac6-native`; aucun PPC généré n'y est câblé.

Après rotation, reprendre par le startup natif et localiser le point partagé de
charge des ressources frontend. Ne pas ajouter un shim ou une abstraction avant
d'avoir prouvé que cette route consomme réellement le contrat.

## Resume checkpoint

The native loading integration gate is closed. Do not implement the guest asynchronous provider or an environment shim in the current native route: source inspection proves that route is synchronous. Resume with `NEXT.md`: qualify the minimum visible pre-mission frontend contract from existing controller state, imported frontend resources, and render/present consumers.

## Resume checkpoint

Two gates are closed in this rotation. The current native frontend lacks the complete visual producer `FHM/NFH → atlas/metrics → state draw list → framebuffer`. Resume with the PAL NFH glyph producer contract in `NEXT.md`. One more closed gate requires `SESSION_ROTATE` and a stop.

## SESSION_ROTATE checkpoint

Three gates are closed in this rotation:

1. Native loading is synchronous and independent of the guest async provider.
2. The visible frontend lacks the NFH-to-framebuffer visual producer.
3. The NFH producer contract stops at the first unresolved field, `NFH+4`, with `NFH+8` next.

Resume from `NEXT.md`. Run the canonical demo Ghidra scalar/xref/dataflow slice for the NFH magic and the loads at offsets 4 and 8. Do not continue the failed cache-directory hypothesis.

## Branch-stop checkpoint

The new rotation has closed no gates. The canonical NFH constant and its unique direct reader are qualified. Resume only by correcting the `DecompileAt.java` argument order and inspecting `FUN_822E2858`; all discovery work is already complete. The active goal remains unchanged.

## NFH reader branch-stop checkpoint

The NFH recognizer and its sole caller are now classified. Resume at the data reference `0x82027B0C`: resolve its containing vtable and slot, then follow the constructed view whose data pointer is `leaf+0x450`. The current gate is blocked by quota, not by missing external access.

## NFH view table checkpoint

The table identity gate is closed. The canonical table is `0x82027A64`; `0x822CC9F0` is slot `+0xA8`, and `0x822CC378` is slot `+0xB0`. The latter returns a bounded record pointer at `view+0x10 + index*0x20`. Resume with the next consumer of that record and qualify its field layout before any parser or runtime work.

## NFH record-consumer branch stop

The numeric `+0xB0` consumer scan is inconclusive after five batches: 24
dispatches were found, but none is tied statically to vtable `0x82027A64` or
to the record pointer at `this+0x10`. Constructor call sites are listed in
`artifacts/nfh-view-record-layout-gate/constructor-xrefs.txt`; resume by
decompiling only those callers and propagating receiver/vtable identity. The
branch blocker is
`artifacts/nfh-view-record-layout-gate/BLOCKER.md`. No parser, shim env,
Wine, Xenia or runtime was used.

## NFH receiver provenance branch stop

Les appelants de constructeurs ont été dépliés : quatre chemins finissent sur
la table `0x82027A64` après `0x822CC118`, mais aucun n'est relié à un dispatch
`+0xB0`. Les tables dérivées ne fournissent pas le lecteur. Le dernier trace
large a échoué sur le contrat `START END`; reprendre avec des bornes exactes,
puis seulement qualifier le record. Voir
`artifacts/nfh-receiver-provenance-gate/BLOCKER.md`.

## NFH direct consumer checkpoint — CLOSED

Le traceur corrigé a retrouvé 24 dispatchs `+0xB0`; aucune fenêtre post-appel
ne consomme `r3` comme pointeur de record et aucun owner ne matérialise la
vtable NFH. La branche directe est réfutée. Reprendre avec un autre slot ou
une conservation du retour au-delà de la fenêtre, puis seulement revenir au
layout `0x20`. Résultat :
`artifacts/nfh-receiver-provenance-gate/RESULT.md`.

## IB bootstrap static checkpoint — CLOSED BY BOUNDARY

Les symboles générés `0x821B55C0`/`0x821B9BC8` sont présents, mais aucune
adresse bootstrap connue n'est littérale dans l'objet et les stores sont
calculés depuis `PPCContext`. Le ring natif ne voit l'IB qu'après publication.
Reprendre avec l’instrumentation bornée des stores générés, selon
`artifacts/ib-producer-static-resume-gate/BLOCKER.md`.

## Generated-store hook checkpoint — CLOSED BY RUNTIME ATTRIBUTION

Le traceur de fenêtre existant couvrait les stores scalaires mais pas U128;
l’appel U128 est maintenant présent sans modifier l’ordre des octets écrit.
Le build codegen et le test cœur passent. La capture bornée existante attribue
les 48 stores de `0x16AE0980` à `0x821B20A0` et qualifie les 24 paires
PointList. Reprendre par la chaîne writer → publication → renderer/scheduler,
selon `artifacts/generated-store-hook-gate/BLOCKER.md`.

## Record queue writer checkpoint — CLOSED BY STATIC P-CODE

Le scan normalisé a retrouvé le témoin type 0 et les writers types 1–4
(`0x820FF788`, `0x820FF7F8`, `0x820FFA88`, `0x820FFB50`). Le worker
`0x820FFCA0` transmet les slots à `0x820FEFA8`, qui dispatch ces types. Le
prochain test doit qualifier le type effectivement atteint puis sa publication
renderer/scheduler ; aucun nouveau runtime n'est requis pour la découverte des
writers. Voir `artifacts/ib-writer-publication-static-gate/RESULT.md`.

## Resume: record type reachability

Le slice statique `artifacts/record-type-reachability-gate/control-slice-run2.txt`
confirme l'unique corridor `0x8210A1C0:0x8210ADB4 → 0x82117410` sous 18
gardes. Ne pas redécompiler davantage cette fonction. Reprendre par la capture
bornée pré-déclarée dans `artifacts/record-type-reachability-gate/BLOCKER.md`
pour joindre le `param_4`, le type de slot et la publication downstream.

## Resume: observed zero-type worker route

La capture corrigée `record-type-route-run4` ne voit jamais `0x8210A1C0` ni
`0x82117410`, mais voit `0x820FEFA8(r3=0x2EEEFE90)` à chaque tick 2990–3020;
son sélecteur mémoire `*(r3+0x40)` vaut toujours zéro. Le résultat réfute
cette route comme publication type 1–4 dans la fenêtre; frontend toujours
absent. Reprendre par le producteur statique de `param_4` et la fenêtre
d'activation du corridor, sans refaire ce runtime.

## Resume: static source of the corridor selector

`0x82165CC0` est le seul appelant direct de `0x8210A1C0`; il transmet son
argument entrant `r8` via `r26` puis `r6`, et le target calcule
`param_4 = r6 & 0xFFFF`. La source restante est une arête indirecte/vtable;
ne pas relancer le plateau START avant d'avoir qualifié ce slot ou une valeur
du corridor. Voir `param4-producer-static-run5-summary.txt`.

## Resume: AVIObjectDemo static vtable boundary

`0x8200B6FC` est la vtable `AVIObjectDemo`, slot `+4` vers `0x82165CC0`;
les constructeurs `0x82166550` et `0x821674A8` l'écrivent à `objet+0xC`.
La dispatch `0x821710FC` observée reste sur la vtable primaire `0x8200B844`.
Le témoin borné autour de `0x82165CC0` a été exécuté. Il n'a vu aucune entrée
dans la fenêtre START; l'activation globale reste indécidable. Ne pas refaire
ce runtime avant d'avoir qualifié statiquement l'arête virtuelle.

## Resume: bounded AVIObjectDemo witness closed

Le hook compilé ne voit aucune entrée dans `0x82165CC0`, `0x8210A1C0` ou
`0x82117410` pendant START `2990..3021`; 3021 ticks / 2913 PRESENT, frontend
absent. C'est indécidable pour cette fenêtre, pas une réfutation globale; le
hook est confirmé par désassemblage de l'objet généré. Ne pas répéter la même
capture. Reprendre par la qualification statique de l'arête virtuelle, du
récepteur ajusté et de la provenance de `r8`. Voir
`artifacts/record-type-reachability-gate/avi-receiver-run1-summary.txt`.

## Resume: AVI virtual-edge candidate set

Le scan statique trouve 510 dispatchs virtuels slot `+4`, dont 11 dans la
fenêtre locale. `0x82165D6C` est interne à `0x82165CC0` et son call direct
`0x8210A1C0` est à `0x82165D8C`; dix sites restent à qualifier comme entrants.
Les flux globaux testés ne résolvent aucune vtable cible. Reprendre par la
provenance statique de ces dix récepteurs, sans refaire la fenêtre START. Voir
`artifacts/avi-virtual-edge-static-gate/BLOCKER.md`.

## Resume: AVI constructor provenance pass

La vtable `0x8200B6FC` n'est pas un pointeur statique : seuls les constructeurs
`0x82166550` et `0x821674A8` l'écrivent à `objet+0xC`. Le global
`0x82731A30` est construit par `0x82373090`, mais `0x821710FC` lit la vtable
primaire `0x8200B844`; les flux globaux connus ne fournissent donc pas le slot
`0x82165CC0`. Les dix dispatchs locaux restent à propager individuellement.
Ne pas refaire START. Reprendre avec
`artifacts/avi-virtual-edge-static-gate/BLOCKER.md` et
`constructor-flow-final.txt`.

## Resume: AVI candidate caller census

`0x821600C8` est un dispatch générique sur `param_1`; `0x8216B3B4` n'a pas de
fonction contenante. Les callers directs récupérés sont listés dans
`candidate-callers-run1.log`, mais aucune vtable n'est encore jointe à
`0x8200B6FC`. Le gate a atteint sa limite de lots : décompiler ces callers à la
prochaine reprise, sans runtime ni shim env.

## Resume: AVI candidate refutation

`0x821A4454 → 0x82169B38` utilise le receiver statique `0x82390574`, dont la
vtable vaut `0x8200BAEC`; le candidat `0x82169B7C` est donc exclu. Les deux
`bctrl` signalés vers `0x821600C8` sont en réalité des appels
`PTR_PTR_82390034[+8]`. Reprendre par les receivers state/manager restants et
leurs vtables concrètes, sans runtime.

## Resume: manager/accessor provenance boundary

Les receivers restants sont indexés dans `manager-factory-summary-run5.txt` :
`FUN_82327108()+0xB5BC`, `FUN_82327104()+0xD274`,
`FUN_82327100()+0x7C`, `FUN_8232710C()+0x68`, `PTR_DAT_8238FEF4` et la route
globale primaire. Les stubs `FUN_82327100/0C` sont vides dans Ghidra, donc la
provenance est indécidable, pas nulle. Reprendre par la matérialisation
statique de ces retours; ne pas relancer START ni ajouter de shim env.

## Resume: ABI helper correction

`FUN_82327100/04/08/0C` sont les entrées d'un helper PPC de sauvegarde, et
préservent le `r3` entrant; ils ne renvoient pas de managers. La prochaine
reprise doit donc propager `r3` depuis les callers des sites virtuels. Le
global `PTR_DAT_8238FEF4` pointe vers `0x82731150`, nul dans l'image importée,
et reste un candidat séparé à qualifier par son producteur. Ne pas relancer
START. Preuve compacte :
`artifacts/avi-virtual-edge-static-gate/helper-boundary-correction-run8.txt`.

## Resume: indirect table boundary

La table `0x8200C5C0..0x8200C664` contient `0x8216F7F8` mais ne possède aucun
RTTI valide, aucune matérialisation PPC de sa base et aucune occurrence U32 de
son adresse. Elle reste une table de dispatch non reliée à un objet AVI.
`0x823270F8` est l'entrée ABI `r24`, donc le receiver de `0x82165230` reste le
`r3` entrant. Reprendre par le premier waiter/producteur guest documenté,
sans relancer START.

## Resume: caller propagation checkpoint

Le census `caller-direct-census-run9.log` et `parent-caller-census-run9.log`
qualifie cinq appels directs et aucune arête directe vers `0x8216F7F8` ou
`0x82170F58`. `0x82167320` transmet son `r3` à `0x82166AE0`; les fonctions
`0x8216EA20/0x8216ECE0/0x8216F7F8` conservent le `r3` via l'entrée ABI
`0x8232710C`, et `0x8216F7F8` appelle `0x8216F640` avec ce receiver.
`0x82165490` est appelé par `0x821662CC` et passe par `FUN_823270F8()`.
La prochaine reprise doit reconstruire la vtable référencée par la donnée
`0x8200C624` et qualifier `0x823270F8`; aucun runtime.

## Resume: primary counter contract closed

Le gate suivant est fermé par preuve statique. L'état `0x82934708` est le
receiver commun : `0x822E3EC0` publie `state+0x50` via `0x822EEE10`, et
`0x822E40E8` le consomme avec les comparaisons `>=` puis `>`. Le seuil
`state+0x10` est mis à zéro par l'initialisation et par la route startup
`0x822E52D0` (`last_argument-1`, argument `1`); `state+0x18` n'est activé que
par shutdown. Ne pas ajouter de shim env ni relancer START pour cette question.

Reprendre sur la production du draw normal/frontend autour du dispatch
post-`START`. Artefacts :
`artifacts/primary-counter-static-gate/gate.status`, `RESULT.md` et
`artifacts/guest-wait-producer-gate/primary-counter-static-summary-run15.txt`.

## Resume: frontend visual producer gate closed

La state machine native traverse bien `Title -> Mission`, et le cache PAL
FHM/NFH est validé, mais `RetailFrontendResources` ne décode pas les feuilles
NFH et aucun renderer ne reçoit `FrontendState`/les ressources frontend. La
route `run_play_impl` présente exclusivement la scène mission. Le verrou
suivant est donc le layout d'une feuille NFH PAL et son draw Title réel, pas un
shim env ni un nouveau runtime. Preuve compacte :
`artifacts/frontend-visual-producer-static-gate/RESULT.md`.

## Resume: listener `+0x20` gate closed

Le sweep de `CSwgListener::+0x20` sur `ace-combat-6-demo/Default.xex` trouve
89 dérivés, dont 40 non-stub; le snapshot post-START de `M102` n'active
toutefois que deux objets dont le slot est `0x820AC748`. Aucun implémenteur
caché n'explique ce plateau. Reprendre sur une feuille NFH PAL réelle et son
contrat de draw Title. Preuve compacte :
`artifacts/listener-slot20-sweep-gate/RESULT.md`.

## Resume: NFH structural leaf closed

Les leaves PAL extraits établissent `0x450 + count*0x20`; le reader PPC
`0x822CC378` confirme le stride `0x20`. `NFH+4`, le mapping de caractères et
le lien atlas/draw restent indéterminés. Reprendre par le receiver du retour de
`0x822CC378`, pas par un parser ou un runtime. Preuve :
`artifacts/nfh-layout-static-gate/RESULT.md`.
## Resume: NFH direct-consumer gate closed

Le layout feuille NFH est structurellement qualifié et les 24 candidats
`+0xB0` sont réfutés comme consommateurs directs. Reprendre par une arête
indirecte vers l'atlas/draw, sans runtime ni modification native tant que le
producteur visuel n'est pas identifié.
## Resume: canonical NFH PPC accessor closed

`0x822E2858 -> 0x822CC9F0 -> 0x822CC378` is now qualified in the canonical
Ghidra project: `NFH\0` recognition, `leaf+0x450` base, and `index*0x20`
record access. `leaf+4` has typed read sites but no atlas semantics. Resume by
finding the first visual consumer/draw contract, not by adding a parser, shim,
or runtime trace. Evidence:
`artifacts/canonical-ppc-nfh-consumer-slice-gate/RESULT.md`.
## Resume: title resource-manager branch closed

`CModeTaskTitle` and `CResourceManager` are now qualified. Key formatting,
lookup/allocation, flag initialization, and `0x822CC2A8` pointer storage do not
decode FHM/NFH or create draw data. Resume at the child resource/view vtable
that could feed `0x822CC9F0/0x822CC378`; do not add a shim or parser yet.
Evidence: `artifacts/frontend-compact-title-next/RESULT.md`.
## Resume: child resource/view metrics branch closed

`0x822CC7A8/0x822CC8E8/0x822CC988` prove the child/view → NFH metric edge;
`0x822CCD98` remains scalar, while `0x822CCCF8` copies a 0x20-byte record via
the optimized copy helper `0x82327D90`. `0x821A00E8` only queues title
resources. No atlas, draw-list, PM4, framebuffer, or present edge is present.
Resume by tracing the first title-side consumer of those metrics toward a
renderer/compositor buffer. Do not add a parser, env shim, or runtime capture.
Evidence: `artifacts/child-resource-view-next/RESULT.md`.

## Resume: first non-bootstrap RT0 writer closed

The indirect renderer method `0x822F84E0` is now ABI-qualified through
`0x821B6708`, `0x821B6078`, `0x821B5B10`, `0x821B58B0`, and the vertex/packet
builder `0x821B55C0`. Its object color field reaches the packet and the
floating record reaches vertex data. Resume with one bounded probe of slot
invocation and RT0 change; do not implement an env shim or optimize yet.
Evidence: `artifacts/rt0-writer-callers-next/RESULT.md`.

## Resume: render queue consumer is next

The qualified RT0 writer remains downstream of an unconsumed render queue.
At the latest complete report, producer activity is nonzero but consumer and
PM4 submission remain zero; the targeted writer probe sees only bootstrap
PointList draws and no resolve. Resume with a static producer/consumer slice
around `0x82386CC0`, then one bounded queue-counter probe. Do not implement an
environment shim, NFH parser, or optimization.

## Resume: queue control closed; payload producer is next

La queue `0x82386CC0` n'est pas bloquée : `0x820FF710` publie, `0x820FFCA0`
consomme/réinitialise et appelle `0x820FEFA8`. Le compteur consommateur nul
était un artefact de post-reset. Le record copié vers cette entrée reste nul
(`+0x40..+0x58`), tandis que `0x820FF788`/`0x820FF7F8` sont les writers
statiques de types 1/2 non atteints dans la route active.

Reprendre par leurs appelants/vtables et par le producteur du premier payload
non nul. Preuve : `artifacts/render-queue-consumer-gate/RESULT.md`.

## Resume: corridor writers qualifié, activation amont ouverte

La vtable `0x82008EF0` expose les writers type 1–4; `0x820FF788` est atteint
par `0x82117410`, appelé directement seulement depuis `0x8210A1C0`. Le
sélecteur `param_4` et les lookups de tables gardent cette route. La capture
bornée déjà faite n'y entre pas et voit `record_type=0` aux appels du
consumer.

Reprendre par la provenance statique de `param_4`/du récepteur virtuel. La
route queue-control est fermée; aucun shim env, parser NFH, appel direct writer
ou `-O3` avant fermeture de ce gate. Preuves :
`artifacts/record-type-reachability-gate/RESULT.md` et
`artifacts/render-queue-consumer-gate/RESULT.md`.

## Resume: wake-producer bit-4 gate closed

The unique indirect `0x822F85B8` path constructs the state object through
`0x821BB4C8` with `r6=1`, yielding `0x0C000001` at `+0x56F8`. The exhaustive
generated scan finds no later writer for bit `0x4`; `0x821C64E8` is subsystem
initialization, not a state-word producer. Resume with a binary-qualified
Ghidra search for an alternate indirect/imported constructor or service. Do
not patch the selector/time/state, add an env shim, or optimize yet.
Evidence: `artifacts/wake-producer-gate/RESULT.md`.

La fausse occurrence `0x8207DF10` est `.pdata` (métadonnées de fonction),
pas un slot de dispatch; le gate `pdata_false_alternate_constructor` est
fermé. Reprendre à la frontière du producteur importé/omis, sans revenir sur
ces lignes `.pdata`.

## Resume: chaîne CX360UnitManager → ring qualifiée

Le slot `+0x14` de `CX360UnitManager` (`0x820A45E0`) lève `(17,6)` à
`0x820A4778`; `0x821ADAB8` est le seul écrivain de `device+0x5460`, champ
testé par `0x821C57D0` avant la soumission. Le scan frais ne trouve aucun
appel direct au slot/callback. Les sept constructions d'instance recensées
restent hors du parcours natif : reprendre par leur garde mission ou le
service/import qui doit les activer.

Ne pas ajouter de shim env, appel direct, patch selector ou `-O3` avant le
test discriminant `(17,6) → +0x5460 → soumission`. Preuve :
`artifacts/cx360-unit-manager-gate/RESULT.md`.

## Resume: CX360 aval du provider de readiness

`0x8217C4D8` est un callback de `CModeTaskGame*`, installé par
`0x8217C678`/`0x82173DF0`; son bras `-3` choisit ensuite les constructeurs de
mission et de `CX360UnitManager`. La route active ne les atteint pas parce que
le chargement reste bloqué plus tôt dans `0x8219DF00`/`0x8219F5D0`, sur les
attentes `0x8219AF20` et `0x82195B50`. Reprendre par le producteur du statut de
readiness et son contrat natif. Ne pas ajouter shim, appel direct renderer ou
`-O3`. Preuve :
`artifacts/cx360-construction-activation-gate/RESULT.md`.

## Resume: provider atteint, verrou post-provider ouvert

La capture bornée du chemin recompilé observe l'objet `0x2E3B0040` aux ticks
`4116..4123`: `state=0 → 1`, `0x8219AF20` présent, `0x82195B50` absent, retour
`0 → 1`. Le plateau reste sans frontend/mission à `5600` ticks. Reprendre par
la transition qui consomme cette fin puis le producteur du premier payload de
rendu non-bootstrap. Ne pas implémenter shim env, appel direct, parser NFH ou
optimisation. Preuve : `artifacts/readiness-provider-gate/RESULT.md`.

### Dernier checkpoint — post-provider partiellement qualifié

La capture `artifacts/post-provider-transition-gate/RESULT.md` montre que le
travail `0x2E3B0040` progresse `+0x0C:1→2→0`; `0x8219AF20` est appelé,
`0x82195B50` non. Le `result=1` du watcher est le retour de l'appel virtuel
`0x8219DF00`, non celui de l'agrégateur `0x8219F5D0`. Le runtime plafonne
toujours sans frontend/mission. Reprendre par l'observation ciblée de
`0x8219F5D0`/`CTaskLoading`, sans shim env ni `-O3`.

### Checkpoint — agrégateur fermé, réveil aval manquant

Le résolveur `0x8219EE40` mappe la clé `0x5908E2C8` au manager `0x18BB0100`.
`+0x20=0x18BB0120` est drainé jusqu'à zéro et `+0x24=0x18BB0124` est remis à
zéro au tick 4123. Cette fin ne réveille toujours aucun thread : le runtime
termine à `4140` ticks, `4032 PRESENT`, `23 blocked`, sans frontend/mission.
Reprendre par le consommateur aval du store de statut ou la FSM de readiness,
sans shim env ni `-O3`. Preuve : `artifacts/post-provider-transition-gate/RESULT.md`.

### Checkpoint — transition aval confirmée, attente primaire active

À `tick=4123`, le task `0x2E3D0080` appelle `0x8216CB40` via le slot 15,
écrit `+0x0C=1` et s'inscrit dans `0x826DF804`. Le gate de transition aval est
fermé, mais le scheduler finit à `4140` ticks avec `23 blocked/0 runnable`,
sans frontend ni mission. Reprendre par le listener et la reprise de
`0xE000004C` (`0x821A8C88`/`0x821A69CC`), puis le premier draw non-bootstrap.
Preuve : `artifacts/readiness-consumer-gate/runtime-consumer/`.

## Resume — listener/post-wake n'est pas le verrou

`0x826DF804` est un slot du tableau de listeners, pas le début d'une chaîne
causale vers `E000004C`. Les callbacks indexés `+0x20`/`+0x54` sont déjà
qualifiés, et le waiter `E000004C` reprend jusqu'à `0x82327154`. Reprendre par
le producteur statique de `param_4` pour le corridor
`0x8210A1C0 → 0x82117410 → 0x820FF788`, sans nouveau runtime.

## Resume — gate r8/record utile bloqué

Le dispatch `0x821710FC` ne cible pas l’AVI slot+4; l’owner qui fournit la
vtable `0x8200B6FC` et le `r8` entrant de `0x82165CC0` reste à qualifier.
`0x820FEFA8` ne rejoint pas statiquement la chaîne RT0/Xenos. Un probe de
8 000 ticks a produit des PRESENT mais aucun paquet soumis/décodé et aucun
jalon frontend. Reprendre par un slice statique owner/r8, puis par la
première émission pixel, sans shim ni appel forcé.

## Resume — seam causal Edge prêt, contrat toujours ouvert

Le build source Edge est dans `.tools/xenia-edge-source/build-agent-clang`.
Pour reproduire la sonde, définir `XE_AC6_CAUSAL_TRACE` et activer
`--cpu_trace_mask=4 --trace_gpu_stream=true` sur le XEX PAL qualifié, avec un
cache host neuf et une fenêtre bornée sous cgroup. Les runs du 22 août n'ont
pas atteint `0x82386C58`, `0x82165CC0`, `0x8210A1C0` ou `0x82117410`; ne pas
interpréter cette absence comme une preuve globale. Reprendre par la voie
statique owner/r8/record documentée dans
`artifacts/xenia-edge-causal-gate/RESULT.md`.

## Resume — writer provider qualifié

Le provider remplit effectivement ses sous-tables : `0x82114798` obtient un
count de ressource, écrit les slots et `entry+0x04`, puis lance les appels de
résolution/attente. Cette preuve est uniquement statique et ne ferme pas la
provenance de l'owner AVI ni du `r8` entrant à `0x82165CC0`. Reprendre ce
dernier bord dans
`artifacts/static-owner-shared-gate/provider-entry-contract-decomp.log`.

## Resume — validations locales passées, blocker inchangé

Builds codegen-OFF/ON, CTest ciblé, runners différentiels EDRAM/writeback,
audit screencap et installation package sont passés; `bin/bin` est absent et
aucun XEX/PAC/TBL/Ghidra/C++ généré n'est installé. Cette lane ne prouve pas
un draw guest. Reprendre par l'owner AVI et la provenance de `r8` avant toute
modification renderer ou nouvelle trace runtime.

## Resume — blockers owner/records qualifiés

Le writer provider `0x82114798` et son layout sont fermés statiquement. Les
records 1–4 s'arrêtent, dans la fermeture directe, au service motion CPU;
callbacks `[entry+0x20]`/`[entry+0x1c]` restent à résoudre. L'owner AVI
`base+0xC → 0x8200B6FC` et le `r8` entrant à `0x82165CC0` ne sont pas reliés
par les consommateurs inspectés. Reprendre par le producteur de la liste
`state+0xB5B4/B5B8`, sans patch runtime.

## Resume — writer d'état fermé, owner secondaire encore ouvert

`0x82165E68` remplit `state+0xB5B4/B5B8` depuis les clés de configuration
`param_2+0x12/+0x16` et active `DAT_826F6188`. Il reste à qualifier les valeurs
de ces clés et l'instance portant `0x8200B6FC`, dont `r8` alimente le sélecteur
de `0x8210A1C0`. Reprendre statiquement ce lien avant toute nouvelle exécution.

## Resume — owner AVI et callbacks indirects toujours non qualifiés

Les seuls stores statiques de `0x8200B6FC` à `object+0xC` sont dans
`0x82166550`/`0x821674A8`; aucun appelant inspecté ne relie cet objet à
`0x82165CC0` ni ne définit son `r8`. Les six callbacks records restent des
champs de table data-driven et leurs wrappers n'atteignent pas PM4/Xenos dans
la preuve disponible. Reprendre par les initialiseurs de configuration/table;
aucun patch renderer ou runtime global n'est autorisé.

Le groupe prioritaire de dispatchs AVI est épuisé : aucun des quatre sites
inspectés ne fournit `owner+0xC → 0x8200B6FC` ni un `r8` attribuable à
`0x82165CC0`. Le prochain travail statique doit remonter les initialiseurs de
configuration et de table; ne pas relancer le runtime identique.

Le census de `0x82165E68` n'a trouvé aucun caller direct qualifié ni écriture
des champs `param_2+0x12/+0x16`; le contrat amont est désormais nommé comme
objet/producteur de clés manquant. Les preuves restent statiques et le runtime
ne doit pas être répété à fenêtre identique.

## Reprise après fenêtre import (2026-08-22)

La fenêtre bornée `import-window` a réfuté la section critique comme cause :
`0x8219AF20` effectue un lookup borné et les imports sont tous traités. Ne pas
répéter ce runner ni modifier le scheduler. Reprendre par la qualification
statique du créateur de `param_2` et de la publication AVI `base+0xC/r8`, puis
revenir au premier record non nul avant toute extension renderer.
Preuve : `artifacts/goal-playable/runtime-frontier/import-window.md`.

## Reprise — dernier bord du corridor record (2026-08-22)

Le consumer et les writers de la file sont qualifiés; ne pas leur ajouter de
bridge. Reprendre par l'opaque owner AVI `0x8200B6FC/+4` et la définition de
`r8 → param_4`, puis vérifier les cinq ressources du selector. Tant que cette
arête ne livre pas un record type 1–4 non nul, aucune modification Xenos ni
capture menu supplémentaire n'est justifiée.
Preuve : `artifacts/goal-playable/record-population/RESULT-followup-next.md`.

## Reprise — census AVI final (2026-08-22)

Le headless final n'a trouvé aucun owner ajusté qualifié ni caller direct de
`0x82165CC0`; les deux constructeurs restent les seuls stores de
`0x8200B6FC` à `+0xC`. Reprendre uniquement par l'île indirecte et le
producteur de `r8`; ne pas toucher au renderer, au scheduler ou aux imports.
Preuve : `artifacts/goal-playable/avi-owner/RESULT-final.md`.

## Reprise — seam Edge exécuté, payload toujours absent (2026-08-22)

Le seam d'observation Linux de `MAIN_THREAD_PROMPT` est construit et vérifié,
mais les trois runs Edge expirent avant les PCs nommés; ne pas transformer
Xenia en dépendance runtime. Côté natif, le probe borné atteint le tick 1220,
avec queue active mais 2048 slots nuls, aucun type 1–4 et aucun PM4. Reprendre
par la table/owner AVI et la provenance de `r8`, uniquement avec les artefacts
statiques existants; aucune nouvelle fenêtre runtime identique ni modification
renderer n'est justifiée.
Preuves : `artifacts/xenia-edge-causal-gate/RESULT.md` et
`artifacts/goal-playable/runtime-frontier/RESULT-current-run.md`.

## Reprise — table séparée, call-site AVI restant (2026-08-22)

La plage `0x8200C5C0..0x8200C664` est une vtable indépendante (`0x8200C614`),
pas l'owner `0x8200B6FC`. Le dernier état fiable du corridor est le dispatch
virtuel `0x82165D70`, avec receiver depuis `r4` entrant et sélecteur depuis
`r8` via `r26 → r6`; le producteur indirect reste à qualifier statiquement.
Ne pas modifier le renderer ni injecter `param_4=1`. Preuve :
`artifacts/goal-playable/avi-owner/RESULT-dispatch-table.md`.
# Reprise — 22 août 2026

Dernière mesure : `runtime-current-20260822` atteint 1622 ticks puis timeout,
sans record visuel, AVI ou PM4; seules les écritures de pointeurs de queue
apparaissent. Les voies ACC (`0x8219EE40`/`0x8219F5D0`) et AVI
(`0x82166550`/`0x821674A8`) restent négatives. Reprendre par la jonction
statique `0x821ADAB8`/`0x821ADC78 → device+0x5460`, sans injection ni
modification du renderer avant une arête qualifiée.

## Reprise — renderer titre multi-frame fermé (2026-08-22)

La cible titre Vulkan est persistante et les slots naturels P1/P2/P0 rendent
jusqu'au tick 330 avec readback CPU/Vulkan exact; les sept tests ciblés
passent. Les images sont non noires et suivent le fade guest, mais restent des
aplats bleus parce que toutes les soumissions qualifiées utilisent encore la
même texture BC3 64×64 à `0x0DF22000`. Reprendre statiquement par la
condition/producteur de la première soumission après l'IB P0
`0x12ACA8C0`, puis joindre le fetch réel à `brandLogo/003_NTXR`. Ne pas
substituer l'asset ni élargir l'allowlist sans tuple exact.

## Reprise — capture native noire, Q non produit (23 août 2026)

La capture native la plus récente est noire :
`artifacts/goal-playable/brandlogo-slotfix-runtime-20260823/title.png`.
Le dump guest séparé montre bien BANDAI NAMCO, mais n'est pas natif :
`artifacts/goal-playable/brandlogo-texture-dump-runtime-20260823/title-512x512.png`.
Le SDK confirme que le slot 94 est correctement adressé avec `+94*2`.

Reprendre par l'owner/list record qui doit produire type 4/5/6 et Q1, puis
par la publication naturelle handle `0x59`/record #2. Ne pas recopier P vers Q
ni modifier le renderer tant que cette arête guest n'est pas qualifiée.

## Reprise — resource-index fermé (23 août 2026)

Le run borné `brandlogo-resource-index-runtime2-20260823` confirme le retour
guest : slot 6=`8`, slot 21 type=`15`, sélection `+4`, index owner=`0`, puis
réinitialisation de `owner+0xD8`. Le second record n'est toujours pas produit.
Reprendre par le producteur statique du record type 4/5/6, le compteur de liste
et le handle `0x59` vers Q1; conserver le renderer fail-closed et ne pas
recopier P vers Q.

## Reprise — liste SWG et record #2 non joints (23 août 2026)

La ressource `003_NTXR`/handle `0x0E000059`, le consumer
`0x82326420` et le callback Q `0x82119048` sont qualifiés. Le manque causal
reste l'énumérateur qui doit relier `0x1AA48` à `0x820D18C8/0x820D16A8`, faire
progresser l'owner `0x2E3CED10`, puis publier count>=3 et record #2. Reprendre
statiquement par cette arête ; ne pas modifier renderer, recopier P→Q ni
forcer le callback.

Preuve : `artifacts/goal-playable/brandlogo-frame-owner-static-20260823/RESULT.md`.

## Reprise — factory SWG bornée, screencap noir (23 août 2026)

Dernier probe : `artifacts/goal-playable/swg-factory-return-runtime-20260823/`.
La factory naturelle retourne les owners enfants à T222/T225/T402, mais ne
publie aucun record #2/handle `0x59`; la borne 500 finit à `frontend=false`.
Le dernier screencap audité est
`screencaps/ac6-demo-pal-present-t000000000498-p000000000188-s000000000513-0c660f2bd3eff315.png`
et reste entièrement noir. Reprendre par le chargeur/interpréteur SWG qui
doit produire la liste de trois records ; conserver le renderer fail-closed.

## Reprise — writer Q1 et matérialisation SWG non qualifiés (23 août 2026)

Dernières passes : `swg-interpreter-static-followup-20260823` et
`title-vertex-writer-followup-20260823`. Elles réfutent `0x820E50D8/0x820E5140`
comme matérialisateurs de liste et laissent `0x82119048` au statut
`CANDIDATE_ONLY / JOIN_OPEN`. Le dernier screencap reste le PNG noir référencé
ci-dessus. Reprendre par l’entrée naturelle du record/Q1 ; le renderer reste
fail-closed et aucune donnée Xenia ne doit devenir une dépendance runtime.

## Reprise — route record précoce confirmée vide (23 août 2026)

La trace bornée `record-route-early-runtime-20260823` atteint 300 ticks et
observe 32 sondages de `0x820FEFA8`, tous `record_type=0` et
`record+0x10c=0`; le corridor `0x8210A1C0/0x82117410/0x820FFCA0` n'est pas
appelé. Le Q1 writer n'a donc aucune entrée naturelle et le dernier screencap
reste noir. Reprendre par la matérialisation SWG/ASContext qui doit publier
record #2/handle `0x59`; ne pas toucher au renderer ni copier P vers Q.

Preuve : `artifacts/goal-playable/record-route-early-runtime-20260823/RESULT.md`.

## Reprise — materializer SWG→ACC non joint (23 août 2026)

La passe `swg-materialization-followup-20260823` ferme seulement le producteur
du curseur/frame actif (`0x820E50D8/0x820E5140 → 0x82322D20`).
`0x82323BB8` consomme/publie les frames existantes et `0x823246C0` ne fait que
dispatcher les opcodes ASContext. Le record #2/handle `0x59` et les stores Q1
restent absents ; reprendre par le receiver virtuel SWG→ACC ou un writer de
`owner+0x20`/liste, sans toucher au renderer.

## Reprise — récepteur slot 4 connu, producteur non joint (23 août 2026)

Le seam SWG est maintenant borné : `0x820064E8` pointe vers `0x820D18C8`,
appelé naturellement depuis `0x8232342C` et `0x82323594` avec le nouveau nœud
et `param_3`. Aucun de ces chemins ne crée la liste de l'owner ni Q1. Reprendre
par le writer de `global_swg_context+0x10` et l'allocation/forme de `param_3`,
puis fermer le lien vers record #2/handle `0x59`; renderer inchangé et
fail-closed tant que ce lien reste ouvert.

Preuve : `artifacts/goal-playable/swg-slot4-receiver-20260823/RESULT.md`.

## Reprise — slot 4 vivant, descriptor join ouvert (23 août 2026)

Le probe `swg-slot4-trace-runtime-20260823` a atteint 300 ticks et observe la
factory `0x820D18C8` puis `0x820D0DB8 → 0x82323808` avec descripteurs `0x0B` et
`0x01`. Il ne voit toujours ni record #2/handle `0x59` ni écriture Q1 ; le
frontend reste faux et le screencap noir. Reprendre par le writer du
descripteur/ASContext vers l'owner titre, sans fallback renderer.

Preuve : `artifacts/goal-playable/swg-slot4-trace-runtime-20260823/RESULT.md`.

## Reprise — writer context+0x10 fermé, descriptor join ouvert (23 août 2026)

La passe `swg-context-writer-static-20260823` qualifie `0x820CF4E8` comme
writer d'initialisation et `0x820D0A00` comme writer de teardown de
`context+0x10`, avec le join owner→contexte via `0x82321E18`. Aucun lien vers
le descripteur titre, record #2, handle `0x59` ou Q1 n'est établi. Le dernier
screencap reste noir ; reprendre par le producteur du descripteur avant
`0x820D18C8`, sans modifier le renderer.

Preuve : `artifacts/goal-playable/swg-context-writer-static-20260823/RESULT.md`.

## Reprise — producteur du blob SWG non qualifié (23 août 2026)

La passe `swg-descriptor-producer-static-20260823` invalide le pseudo-hit
`0x821846A0/0x821846FC` et ne trouve aucun writer de `0x1AA48`. Le constructeur
`0x82326B80` établit seulement `owner+0x20`; `count`, les liens de records et
`record+0x0C==2` restent sans producteur qualifié. Reprendre par le parser du
blob passé à `0x82326B80`, renderer inchangé et screencap noir.

Preuve : `artifacts/goal-playable/swg-descriptor-producer-static-20260823/RESULT.md`.

## Reprise — fetchs titre déjà couverts, Q1 toujours zéro (23 août 2026)

La dernière sonde Vulkan bornée (`title-fetch-trace-runtime-20260823`) valide
les deux fetchs naturels titre et leurs textures, puis produit un readback
1280x720 entièrement noir avec `passed_samples=0`. Les vertices Q1 lus à
`0x104A4890` sont tous zéro ; aucun changement renderer n'est autorisé.
Reprendre par le producteur guest SWG qui doit remplir Q1 et publier le record
#2/handle `0x59`.

Preuve : `artifacts/goal-playable/title-fetch-trace-runtime-20260823/RESULT.md`.

## Reprise — provider population non atteinte (23 août 2026)

La sonde `provider-population-runtime-20260823` a couvert les ticks 190..430
avec `status=4`, sans aucune entrée dans les writers provider qualifiés. Le
worker `0x820FEFA8` reste `record_type=0`; le dernier screencap est donc le
PNG noir t429 référencé dans `title-screencap-vulkan-20260823`. Reprendre par
l'état/appelant guest qui doit publier le premier record avant le corridor
`0x8210A1C0`; renderer et Xenos restent inchangés.

Preuve : `artifacts/goal-playable/provider-population-runtime-20260823/RESULT.md`.

## Reprise — diagnostic de canaux fermé (23 août 2026)

La pile démo encode directement le readback RGBA8 et ne passe pas par la
swapchain BGRA de la reconstruction retail. Le dump source est noir, pas bleu;
la prochaine reprise reste l'amont guest B5B4/B5B8 → provider/Q1.

Preuve : `artifacts/goal-playable/format-diagnostic-20260823/RESULT.md`.

## Reprise — caller de configuration encore ouvert (23 août 2026)

Le seul writer direct de `state+0xB5B4/+0xB5B8` est `0x82165E68`; ses valeurs
dépendent de `param_2+0x12/+0x16` et de `0x821EE130/0x821EE0F8`. Reprendre par
le caller virtuel/constructeur de cette méthode, sans modifier le renderer.

Preuve : `artifacts/goal-playable/provider-state-fields-static-20260823/RESULT.md`.

## Reprise — caller `0x82216498` et contrat `param_2` (23 août 2026)

La vtable primaire `0x8200B63C[+4]` est jointe à `0x82216498 → 0x82165E68`.
Le caller fournit `bVar6`, mais le callee le réutilise comme base structurée
aux offsets `+0x12/+0x16`; aucune provenance des champs n'est qualifiée.
Reprendre par la trace bornée de cette entrée et de ces deux lectures, sans
shim ni état synthétique.

Preuve : `artifacts/goal-playable/caller-param2-static-20260823/RESULT.md`.

## Reprise — caller `param_2` non observé, payload Q1 toujours absent (23 août 2026)

La sonde `caller-param2-runtime-20260823` a couvert le frontend jusqu'au tick
440 sans entrée naturelle dans `0x82165E68`; aucun `+0x12/+0x16` n'est donc
disponible. Elle a confirmé le draw titre à texture non nulle mais quatre
vertices Q nuls et un readback CPU RGBA8 entièrement noir. Ne pas ajouter de
swizzle, de valeur synthétique ni de profil Xenos. Reprendre par le producteur
SWG/record qui doit alimenter Q1.

Preuve : `artifacts/goal-playable/caller-param2-runtime-20260823/RESULT.md`.
# Reprise — après cycle 1796 (24 août 2026)

La promotion pending et le premier tick enfant sont fermés statiquement. Ne
pas chercher une xref constante pour `D=0x2DD7936C` : il est sélectionné dans
la table runtime relocalisée de `B`. Reprendre avec l'observation bornée décrite
dans `NEXT.md` et
`artifacts/goal-playable/title-child-initial-frame-static-20260824/RESULT.md`.

Le premier effet persistant après `0x82325288` reste la frontière. Les statuts
restent `frontend=false`, `mission=false`, `terminal=false`,
`supported=false`.
# Reprise autoritaire — après cycle 1799

Ne plus chercher statiquement les mots de `table[13]` : cinq lots ont établi
qu'ils n'existent dans aucun artefact qualifié conservé et que le leaf compact
ne représente pas directement la table relocalisée.

Prochain gate : snapshot read-only au tick 3001, après le START naturel du
tick 3000. Pour le sous-enfant `0x2E3F1350`, lire
`0x2DD796A4/0x2DD796A8`, calculer la liste depuis `0x2DCB1220`, dumper son
compte et huit records maximum, puis résoudre leur type dans la table
`0x8264D074`. Aucun A/B et aucune écriture guest.

Preuves de départ :
`artifacts/goal-playable/title-list13-static-boundary-20260824/RESULT.md` et
`reports/cycle-1799-demo-list13-static-boundary.md`.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.
# Reprise courante — cycle 1810

Le retour HSIO correct est 1. Il restaure le ring (502 soumissions/4405 dwords)
et 407 présentations à 1160 ticks. Ne pas fusionner l'ancien comportement
HSIO=0 : il activait le fallback invité `0x821BA130` et masquait la route GPU.

La capture finale est géométriquement complète mais visuellement invalide :
logo Namco bleu sur fond bleu, alors que le fond doit être blanc; R et G sont
nuls partout. Reprendre statiquement sur le dword vertex couleur
`ffff0000`/`bfff0000`, le fetch `FMT_8_8_8_8 k8in32`, le swizzle
`r2.zyxw`, l'export interpolator1 et le `mul` du PS. Distinguer ce chemin du
format RT/readback avant un unique run visuel codegen-on.

Preuve : `artifacts/goal-playable/hsio-static-gate-20260824/RESULT.md`.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.
# Reprise cycle 1815 — résoudre la table indirecte de `0x82118D18`

Le writer P type 1 `0x82118D18` lit la couleur à `r4+0x18`, mais aucun appel
direct n'est matérialisé : seul son propre HIR contient l'adresse et l'atlas
canonique indique `direct_calls=[]`. Le gate a atteint la limite de cinq lots
avant d'identifier le producteur.

Reprendre statiquement par un scan des dwords big-endian `0x82118D18` et des
cibles de branches PPC dans l'image demo qualifiée. Pour une cellule de table,
remonter son consommateur `mtctr/bctrl`, son index/type, puis la définition de
`r4` et le store vers `+0x18`. N'utiliser un watchpoint que si cette table ne
peut pas être résolue, avec record et fenêtre exacts préparés d'abord.

Preuves :
`artifacts/goal-playable/title-p1-color-producer-static-20260824/BLOCKER.md`,
`static-search.txt`, `target-xrefs.txt`. Aucun code ni runtime modifié.
`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.
# Reprise cycle 1816 — table indirecte P1 localisée

Le `basefile` qualifié contient deux entrées de `0x14` pour le writer
`0x82118D18` : `0x82009E8C` et `0x82009EB4`, fonction à `entry+0x0C`.
Discriminants bruts `+0x04=09000001` et `09000003`; champs communs
`+0x00=0D`, `+0x08=826F61C0`, `+0x10=821187A8`. Il n'existe aucune branche
directe ni construction des adresses de cellules exactes.

Reprendre par les xrefs de la base/du voisinage `0x82009E8C`, identifier le
calcul `index*0x14`, le load `+0x0C` et le `mtctr/bctrl`. Suivre ensuite `r4`
jusqu'au store `+0x18`. Aucun watchpoint avant épuisement de cette frontière.

Preuves :
`artifacts/goal-playable/title-p1-dispatch-xref-static-20260824/BLOCKER.md`,
`basefile-qualification.txt`, `qualified-xref-scan.txt`. Aucun runtime ni code
modifié. `frontend=false`, `mission=false`, `terminal=false`,
`supported=false`.

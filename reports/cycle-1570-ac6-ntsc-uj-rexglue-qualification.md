# Cycle 1570 — qualification AC6 NTSC-U/J pour l'oracle ReXGlue moderne

Date : **2026-08-12**. Cible produit inchangée : AC6 PAL, Mission 01, C++
natif Linux. L'image NTSC-U/J sert uniquement à qualifier l'oracle
`AC6_recomp` moderne et le futur producteur de replay d'entrée poll-exact.

## Décision

L'image possédée localement correspond bien à la famille attendue par
`AC6_recomp` `v1.0.0-beta.1`. Son `default.xex` exact a été extrait en privé,
identifié avec deux outils indépendants, importé dans un projet Ghidra frais et
accepté par le codegen ReXGlue épinglé.

Ce résultat passe l'oracle moderne de « région supposée » à
**`provisional-rexglue` lié à un XEX exact**. Il ne devient ni
`retail-qualified` pour PAL, ni produit, ni preuve audiovisuelle. Les deux
exécutions déterministes du codegen sont identiques ; le runtime Linux complet
ne construit toujours pas sans modifications et n'a pas été lancé.

Le manifeste metadata-only durable est
[`analysis/oracle/ac6-recomp-ab90b-us/identity.json`](../analysis/oracle/ac6-recomp-ab90b-us/identity.json).
Aucun octet ISO, XEX, PAC, C++ généré, icône ou shader n'est conservé dans le
dépôt.

## Identité qualifiée

| Élément | Valeur |
|---|---|
| disque privé | label `XGD2DVD_NTSC`, 7 835 492 352 octets, SHA-256 `204c5e645d79da8776699c12f17bd069f869fbdb10ae79015d1a0ef2b743c98c` |
| module | `default.xex`, 7 483 392 octets, SHA-256 `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc` |
| identité Xenia | XXH3 `892639B654015428`, Title ID `4E4D07D1`, Media ID `531C30BE`, Savegame ID identique |
| exécution XEX | version/base `v0.0.0.8`, disque 1/1, masque région `0x0000FDFF` |
| image | base `0x82000000`, taille `0x00AA0000`, entrée `0x821F5ED0` |
| provenance PE | timestamp `0x46EB93E2`, nom original `ACE6_X360.exe` |

Le masque et le libellé du disque décrivent une édition NTSC-U/J ; le README
du port dit « US only ». Le registre emploie donc l'identité binaire précise et
conserve l'affirmation du projet séparément, au lieu de renommer arbitrairement
le média « US ».

### Contrôles indépendants

- Xenia Canary épinglé au commit
  `16e1eb8e28a2935b75c36707b585a4f5e174ad43`, binaire Linux SHA-256
  `134187430ad32e482b0527e8de10c8ae6371c115bfdfe1f841322d3c6bd9f37a`,
  a relu l'identité et les headers ci-dessus ;
- le lancement était isolé sous Xvfb avec `SDL_AUDIODRIVER=dummy`. Le titre a
  demandé son arrêt avant le gameplay : cette exécution ne prouve aucune frame,
  entrée, cadence ou parité ;
- un import Ghidra headless frais, projet `ac6-us`, programme `default.xex`,
  langage `PowerPC:BE:64:A2ALT-32addr:default`, a reproduit le SHA-256, les
  sections et 8 163 symboles/fonctions supplémentaires issus du chargeur XEX ;
- les requêtes Ghidra de qualification ont ensuite été faites en lecture seule
  et sans analyse automatique supplémentaire.

## Révision AC6_recomp et codegen

| Élément | Pin |
|---|---|
| dépôt | `sal063/AC6_recomp` |
| commit/tag | `ab90b54713e5889f33eee1cc8681dae89fe83d1e` / `v1.0.0-beta.1` |
| arbre | `1e60427e316a2667d189eb1e067a8ec7d776fd50` |
| SDK ReXGlue vendored | arbre `73589e54e95291a7039de6beada6390ac7c12f78` |
| configuration | SHA-256 `c39ffcf1cd4173bfee92bd3be9137808b415a664257b948a1974d305a704fa4e`, 10 527 entrées fonction |
| preset | SHA-256 `2981a545f4a351c2a4b084450258f58409a2ca00a68bc96d6334601cded0fa71` |

Le preset Linux a été configuré avec Clang 21.1.8, les noms `clang-20`
annoncés n'étant pas disponibles sur l'hôte qualifié. Le premier build du tool
`rexglue` a échoué au link : `rexsystem` référence la CVar
`FLAGS_ac6_fix_trails_storage_`, définie dans l'archive `rexgraphics`, mais
l'ordre des archives statiques ne permet pas sa résolution.

Une correction **temporaire et non committée** de cinq lignes a placé
`rexsystem`, puis `rexgraphics`, après `rexcodegen`. Son diff a le SHA-256
`73100dae2f3e1725d4013882d1dc823900640aac5d1dc64168f9620719c52b00`.
Elle ne modifie ni le XEX, ni l'analyse, ni la sortie générée ; elle permet
seulement de lancer le tool Linux.

Résultat codegen, reproduit deux fois byte à byte :

| Mesure | Valeur |
|---|---:|
| fonctions générées après gap-fill | 13 274 |
| fichiers générés | 37 |
| octets générés | 64 784 688 |
| SHA-256 arbre généré | `24564a80f4f3366a9d01b51a9b15de5753908908b6f91c3c960d2653a989d14d` |
| SHA-256 `sources.cmake` | `c604796f3ac27f6da171434f59671d81ea5477badab3beb34925f65358dabf72` |

Le codegen a signalé douze mots non décodés à
`0x823CFE20..0x823CFED0`, pas de `0x10`. Le contrôle littéral montre qu'il
s'agit des descripteurs d'ordinals des trampolines d'import XEX, suivis de
`mtctr`/`bctr`. Le générateur produit les wrappers correspondants. Ils restent
une frontière explicitement classée ; le succès du codegen ne transforme pas
ces mots en instructions PPC retail.

## Frontières d'entrée utiles au replay synchronisé

Le projet Ghidra frais et le codegen concordent sur le candidat de marqueur :

- fonction `0x821CA940..0x821CAA87`, 328 octets, prochaine fonction
  `0x821CAA88` ;
- SHA-256 des 328 octets chargés :
  `a4c027fcc05b34b0bb5ad5c8ad6a7f6bd37e2230797549637ee1950338ea390d` ;
- appelant direct `0x821D7B38` ;
- le corps appelle cinq fois la même routine `0x82211E28`, après son
  initialisation locale. Cela soutient le rôle candidat
  `ac6_frame_input_stage`, sans qualifier sa cadence ;
- un hook placé à l'entrée serait par construction `before_input`. Ce contrat
  doit encore être mesuré à l'exécution avant toute projection 30/60 Hz.

Le thunk invité `XamInputGetState` est exactement :

- `0x82390CE0`, 12 octets, SHA-256
  `9fcb5cb10e9f71b1b6eaf6f8c1854155506dee6702d9b6423888f72622d032bb` ;
- appelants directs `0x8234CEDC` et `0x8234CFA0`, donc LR attendus
  `0x8234CEE0` et `0x8234CFA4`.

L'instrumentation doit néanmoins rester dans le service manuscrit
`XamInputGetState_entry` de ReXGlue. Hooker le thunk généré ferait dépendre le
protocole du C++ généré et manquerait des retours XAM précoces. Aucun fichier
`generated/*` ne doit être modifié.

Le schéma replay v3 actuel est strictement PAL. Il ne faut pas lui ajouter un
second target accepté. La migration retenue est une famille v4 séparant :

1. l'identité oracle NTSC-U/J et son marqueur ;
2. la cible native PAL fixe ;
3. le mapping XInput portable et l'unique projection de cadence ;
4. un reçu qui garde `source_lineage_verified=false` tant qu'aucune attestation
   runner n'existe.

Le binaire natif `AC6RTPLY` reste en version 3. Le schéma de trace v2 doit
refuser le reçu inter-région v4 jusqu'à une future trace portant explicitement
les deux identités. Cette séparation empêche de relabeller l'oracle NTSC en
preuve PAL ou d'appliquer deux fois le zero-order hold 30→60.

## Retest Linux du runtime moderne

Après génération et reconfiguration, le build applicatif non modifié échoue à
la première source concernée : `src/ac6_texture_overrides.h` inclut
`d3d12.h` sans garde de plateforme. D'autres sources du même lot contiennent
encore `windows.h`, SEH, `CreateThread` ou `Sleep`.

Verdict opérationnel :

- le **tool de codegen** peut fonctionner sous Linux avec la correction d'ordre
  de link ci-dessus ;
- l'**application AC6_recomp moderne** n'est pas aujourd'hui une lane Linux
  reproductible ;
- Wine reste donc la voie oracle interactive qualifiée à court terme ;
- un port Linux honnête doit séparer les sources D3D12/Win32 dans CMake et
  fournir des implémentations/no-op explicites, puis passer build, tests et un
  boot Vulkan isolé. Masquer les includes ne suffirait pas.

Aucun patch Linux de produit n'est promu dans ce cycle : son rôle est de
mesurer la frontière, pas de créer un fork oracle parallèle avant le replay.

## Taxonomie et impact sur le plan M01

| Résultat | État | Usage autorisé |
|---|---|---|
| identité XEX, codegen déterministe, adresses et octets des seams | `provisional-rexglue` | instrumentation, contrôle ABI/flux, replay diagnostic |
| services CPU/XAM/XMA/Xenos du SDK | `provisional-rexglue` par défaut | accélérer le bring-up, conserver les divergences connues |
| runtime Linux moderne | `documented-unmatched` / build échoué | aucun oracle interactif |
| cadence du marqueur et replay boot→M01 | ouvert | mesure runtime puis raw v4 requis |
| sémantique PAL, audiovisuel et gameplay M01 | ouvert | qualification PAL ultérieure obligatoire |

Aucune des six lanes du checkpoint 2 et aucun gate JF/JV/JP/JG ne sont fermés.
Le gain concret est une base exacte et reproductible pour construire l'oracle
poll-exact moderne, sans injecter le PAL dans une carte d'adresses NTSC.

## Validations

- extraction XGD2 privée read-only ; SHA-256 du média, de l'extracteur et du
  XEX recalculés ;
- headers et XXH3 recoupés par Xenia épinglé ;
- import Ghidra frais puis lectures `-readOnly -noanalysis`, identité cible
  affichée à chaque requête ;
- limites de fonctions, 82 mots du marqueur, 3 mots du thunk et appelants
  directs contrôlés sur les bytes chargés ;
- codegen exécuté deux fois, arbre généré identique ; aucun généré conservé ;
- build Linux non modifié reproduit jusqu'à l'erreur D3D12 exacte ;
- manifest JSON parsé et audit global du registre de confiance à repasser avec
  ce changement ;
- aucun processus Xenia/Xvfb d'essai ni contenu retail destiné à rester actif
  ou présent dans le dépôt.

## Risques résiduels

- le marqueur n'a encore aucun census de cadence ; son nom métier reste un
  contrat candidat soutenu statiquement ;
- aucun raw poll-exact v4 ni producteur ReXGlue moderne n'existe encore ;
- le codegen accepte la carte d'adresses, mais ses 13 274 fonctions ne
  remplacent ni les frontières Ghidra PAL ni les contrôles d'exécution ;
- les divergences ReXGlue déjà consignées (atomiques, barrières, estimations,
  VMX128, ABI HLE, VSCR/FPSCR) continuent de s'appliquer ;
- seule une qualification PAL bornée pourra promouvoir une sémantique au-delà
  de `provisional-rexglue`.

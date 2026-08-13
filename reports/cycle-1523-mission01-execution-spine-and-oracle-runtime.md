# AC6 cycle 1523 — oracle reproductible et spine d'exécution Mission 01

> Mise à jour R0, 2026-08-13 : la lignée PAL `dcd41b…` décrite ci-dessous est
> historique seulement. L'oracle comportemental actif est le NTSC-U/J exact
> `6eefba42…cbbbc` sur `AC6_recomp@ab90b547…`; la cible native reste le PAL
> `acc302…bcde`. Le contrat inscriptible est `ac6.execution-trace.v3` à six
> domaines ; v2 est lisible uniquement comme historique. L'état normatif est
> dans `analysis/mission01-execution-spine.json` et
> `AC6_RECOMP_LINUX_ORACLE_HANDOFF.md`.

## Résultat

Ce cycle remplace le travail diffus par une tranche verticale contrôlée sans
modifier la frontière produit : AC6_recomp et ses sorties restent des preuves
externes, jamais des dépendances du runtime ou du paquet natif.

- le checkout de référence détaché reste propre à
  `dcd41b7457fcac8242f8ef40de83d1719390d5af` ;
- 53 faux départs configurés sont retirés par un transformateur déterministe ;
- les 53 décisions sont couvertes par 36 scripts Ghidra exécutés sur le projet
  canonique `ace-combat-6`, module `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- le codegen produit 56 fichiers, 105 290 952 octets, arbre SHA-256
  `18bb31ba94c4653fc69a1ac32f2464b8c612a7e2b5a5215537212d108bad39cc` ;
- l'ELF x86-64 PIE construit fait 46 889 016 octets, SHA-256
  `815e18930794883d92a47f03a3747e59501ac2f6b9f242ea5bf0bf3029682b8f` ;
- le smoke Vulkan/audio dummy initialise le XEX, le GPU NVIDIA et une
  swapchain 1280x720, puis atteint le nouveau front borné
  `0x8234530C -> 0x8234524C` ; ce front ne ferme aucun gate.

Le registre `analysis/mission01-execution-spine.json` impose désormais six
phases ordonnées : chargement, sortie contrôlée, premier objectif, débrief,
replay puis parité. Il limite le travail au cône exécuté de Mission 01, sauf
régression des lecteurs partagés qui conserve le corpus de quinze missions.
Les lanes simulation, renderer et plateforme possèdent chacune une condition
de sortie explicite.

## Corrections et invariants

La première hypothèse — les 53 retraits de configuration suffisent — est
invalidée par le lien : `d3d_hooks.cpp` appelait encore
`__imp__rex_sub_821DE7D0`. Le contrat Ghidra existant prouve que
`0x821DE7D0` est interne au helper `0x821DE7A8`, pas une entrée ABI ni un bind
de vertex declaration. Le second checkout reçoit donc un patch hôte minimal de
20 suppressions, SHA-256
`75b228bb883052874f441f3600cf7406f1d148d605f3994316630af18ca9c88f`.
Le rebuild lie ensuite l'exécutable sans modifier le checkout propre ni le code
généré.

L'audit du ladder refuse maintenant :

- une matrice M01 plus avancée que son spine ;
- une phase passée avant son prérequis ;
- une phase runtime passée tant que le manifeste oracle conserve
  `capture_status=not-captured` ;
- une preuve absente, déplacée ou dont la taille/hash diffère ;
- un élargissement silencieux hors Mission 01 ou une dépendance produit à
  l'oracle.

`tools/compare_ac6_execution_traces.py` compare des traces normalisées bornées
et retourne la première séquence, le tick et le chemin structuré divergents.
Le comparateur C++ Mission 01 publie également le premier checkpoint, tick et
domaine dépassant son seuil, au lieu de ne conserver que les maxima finaux.

## Validation

- audit complet du manifeste oracle : checkout propre, checkout corrigé, XEX,
  overlay SIMDe, codegen et ELF **pass** ;
- transformateur reproduit exactement la configuration
  `450d6904d2338ddeb5d80f3cf4a420c9cc6853bcff119c27b06f304b472f5086` ;
- comparateur CLI sur trace normalisée : 2 événements identiques, **pass** ;
- tests Python : **112 passed, 14 subtests passed** ;
- CTest produit : **74/74 pass** (les tests retail externe et Vulkan sans
  display restent des skips qualifiés) ;
- gate JF, adresses, artefacts et dérivations : **pass**, 29 adresses et 19
  artefacts contrôlés, aucun gap de dérivation ;
- build/install relogeable et audit source/binaire/staging : **pass**, 211
  sources, un binaire, 79 fichiers installés ;
- TGZ et audit paquet : **pass**, 85 entrées, sans RexGlue, code généré ni
  payload retail.

## Risques résiduels et prochain front

Il n'existe toujours aucune capture AC6_recomp qualifiée de Mission 01. Les
phases B à F restent donc ouvertes. Le prochain travail unique est de fermer
le front `0x8234530C -> 0x8234524C`, puis d'obtenir le premier replay d'entrée
et la première trace runtime qualifiés. Le renderer interactif reste un raster
CPU présenté par Vulkan ; la lane renderer ne passe pas avant soumission GPU
directe des `DrawPacket` retail.

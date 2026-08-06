# Cycle 782 — raccord XAM vers état canonique et réponse physique

Date : 2026-08-04

## Résultat

Le projet Ghidra canonique `ace-combat-6` localise la vraie ingestion XAM dans
la chaîne suivante :

```text
0x823911C0  wrapper XamInputGetState
  -> 0x8234D3F0 / 0x8234D478
  -> 0x8234D378
       raw XInput LY  device+0x4E
       canonical LY   device+0x3E
```

Ghidra Bridge montre que `0x8234D3F0` appelle `0x823911C0` avec
`device+0x44`, puis `0x8234D378`. La décompilation de `0x8234D378` copie
directement les quatre axes bruts `+0x4C/+0x4E/+0x50/+0x52` vers les champs
canoniques `+0x3C/+0x3E/+0x40/+0x42`, avant la séparation des signes.

Le run `bridge` à variable gameplay unique observe le raccord exact au même
timestamp et sur le même objet :

```text
12:56:44.955  XAM packet=109 ly=32767
12:56:44.955  pc=0x8234D378 device=0x8290DE3C
              raw_ly=0x7FFF canonical_ly=0x7FFF
12:56:45.942  XAM packet=110 ly=0
12:56:45.942  pc=0x8234D378 raw_ly=0 canonical_ly=0
```

Les autres axes et boutons restent nuls dans la fenêtre pitch. Ceci ferme le
maillon `XAM -> état canonique LY` avec contrôle nul et retour à zéro.

La réponse physique sur le même joueur/enfant est reproduite une troisième
fois :

| fenêtre | heure | champ copié | bits | float |
|---|---:|---|---:|---:|
| nul | 12:56:44.132 | `child+128 -> player+160` | `0x3E6D823C` | 0,231942 |
| nul pré-pitch | 12:56:44.876 | idem | `0x3E6CDB4C` | 0,231305 |
| pitch | 12:56:45.747 | idem | `0x3F005884` | 0,501351 |
| après relâchement | 12:56:46.748 | idem | `0x3F2BABB4` | 0,670589 |

La dérive nulle vaut -0,000637 ; le premier sample sous pitch change de
+0,270045. Le joueur `0xB2470000`, l'enfant `0xB2470100` et leurs tables
restent stables. Les quatre mots de transform enfant/joueur sont égaux à chaque
sample.

G8 reste `supported_not_qualified` : l'ingestion canonique et l'effet physique
sont directs, mais le consommateur intermédiaire qui transforme `canonical LY`
en commande de l'enfant n'est pas encore identifié.

## Correction de provenance statique

Les anciens rapports issus de `ace-combat-6-corrected` attribuaient l'entrée
joueur à `0x821CE088`, avec agrégation à `0x82215418/0x82215210`. Cette
sémantique est rejetée pour le projet canonique et le XEX qualifié :

- Ghidra Bridge place `0x821CE088` à l'intérieur de `0x821CE030`; le corpus
  littéral qualifié montre à cette adresse une initialisation/constructeur,
  sans appel XAM ;
- Ghidra Bridge exporte `0x82215418` avec une frontière tronquée ; le corpus
  littéral montre une routine d'initialisation de grandes tables, pas
  l'agrégateur décrit historiquement ;
- la chaîne XAM canonique est directement prouvée à `0x823911C0` et
  `0x8234D378`.

Les tests natifs `ac6-input-821ce088-tests` restent des tests de la tranche
historique ; ils ne prouvent plus l'adresse PAL canonique et ne doivent pas
guider un hook runtime.

## Harness et audio headless

Le lancement qualifié exige `SDL_AUDIODRIVER=dummy`. Sans cette variable, un
essai avec la même racine retail est resté à un seul `PRESENT` après
l'initialisation audio. Cet invariant est maintenant inscrit dans `AGENTS.md`.
Deux tentatives antérieures n'ont pas atteint le guest faute de racine retail
correcte ; elles ne constituent pas des expériences gameplay.

## Identité et validation

- run qualifié : `cycle-782-bridge-pitch-canonical-input`,
  12:53:16–12:57:51 Europe/Paris, timeout 270 s plus cleanup ;
- Xvfb privé `:97`, `SDL_AUDIODRIVER=dummy`, aucun Xenia ;
- game root : `workspaces/ace-combat-6/game-files` ;
- lane `bridge`, interventions déclarées
  `save-dialog-synthesis,force-cvars,fallback-allocator` ;
- timing stock : `ac6_performance_mode=false`, `ac6_unlock_fps=false` ;
- runtime commit `b8b03c7a89dc7f23bcd7844d15aa5080d480bf11`, diff suivi
  `fe46948412b4160bfcfe3afe58d38d91aa825560eea22b13a6c3b0bdab71f9da` ;
- probe non suivi `d07f7097fe826ad073f1c92b408266ae3e711c7f6c87568a416ca8159ec7964d` ;
- exécutable `401cc83b491bf1f594df95168a54209e8c33b9eaa09224cd6c35742b1cd94f6a` ;
- log principal `06a27bb4bf2bc072e88f8f2159edb4230bc6c13d345401dabbde2417318e5eff` ;
- workspace commit `442c6dbcd5188fb84b056293a3ce7a000bd20669`, diff suivi avant rapport
  `39f692474356344f3dcbc45070216d2e27dbd962af1ffbf51eb4634b02d96c70` ;
- build runtime sans codegen : succès ; parser `py_compile` : succès ;
- CTest PAL : 63/63, quatre skips attendus, 38,16 s ;
- corpus généré : 54 fichiers, tree `f42fa2c4c1ec3bfb061003ef7074f73881e968ef2719f7f78e59190d1c5af73d`,
  vérifié inchangé après build ;
- inventaire post-run : aucun AC6/Xenia/Xvfb appartenant au projet ; Xvfb
  `:79` et les processus Ski Park Manager étrangers n'ont pas été touchés ;
  Ollama partagé `127.0.0.1:11435` inchangé.

## Prochain test discriminant

À partir de l'objet canonique stable `0x8290DE3C`, suivre les lecteurs de
`device+0x3E` jusqu'au premier store dans un objet possédé par le joueur ou son
enfant. Qualifier statiquement ce lecteur avec Ghidra Bridge avant tout nouveau
probe ; ne pas réutiliser les adresses historiques rejetées.

# AC6 cycle 308 — au-delà du smoke : où le runtime s'arrête réellement

Le cycle 307 a livré un corpus sans aucun `REX_FATAL`, dont le smoke « survit ».
Survivre ne prouve rien : le processus peut tourner sans rien afficher. Ce cycle
remplace le smoke par une mesure de contenu et attribue l'arrêt réel.

**Résultat : le runtime initialise tout le pipeline graphique et n'affiche
aucune image. Zéro image par seconde. Le fil principal invité reste dans le
chemin de démarrage, à décoder du PAC.**

## 1. Instrumentation mise en place

Deux outils, tous deux versionnés :

- `tools/ac6-capture.sh` lance le jeu sur un serveur X privé (`Xvfb`) et capture
  la fenêtre racine toutes les 15 s, puis mesure pour chaque capture la moyenne
  des pixels et le nombre de couleurs distinctes. Un écran noir vaut
  `mean=0, distinct_colours=1`. C'est ce qui remplace « le processus vit ».
- `tools/ac6-stall-bt.py` est un script `gdb` qui lance le binaire, l'interrompt
  après un délai et vide la pile de **tous** les fils. `ptrace_scope=1` interdit
  de s'attacher à un processus déjà lancé ; il faut donc le lancer sous `gdb`.

Découverte d'outillage majeure : le runtime embarque déjà un **panneau AC6
Graphics Diagnostics**, affiché en surimpression. C'est l'instrument faisant
autorité, et il était masqué parce que `ac6_performance_mode` force
`log_level=error`. Toutes les mesures ci-dessous en proviennent.

Les captures elles-mêmes ne sont **pas** versionnées : `*.png` est exclu par la
politique d'actifs du dépôt. Les chiffres relevés sur le panneau sont retranscrits
intégralement ci-dessous ; `tools/ac6-capture.sh` les reproduit.

## 2. Ce que le runtime atteint réellement

Mesuré, pas supposé :

| Étape | État |
| --- | --- |
| Backend Vulkan sélectionné | **oui** |
| GPU retenu | **NVIDIA RTX PRO 4000 Blackwell** (37 descripteurs `/dev/nvidia0`) |
| Chaîne d'échange | **créée, 1920x1080**, format 44 |
| Cibles de rendu EDRAM | **créées, 640x1024 4xMSAA** couleur + profondeur |
| SDL / entrée / audio | **initialisés** (SDL 3.5.0, pilote MnK, backend audio sdl) |
| Système de fichiers invité | **monté**, `game:` et `d:` enregistrés |
| Lecture des archives | **14 Mo** lus depuis `DATA00.PAC`, séquentiels, sans retour arrière |
| Commandes de dessin invité | **26 émises, 26 acceptées** |
| Tampon avant invité | **1280x720** |
| **Dessins hôte** | **0** (1 avec un correctif, §4) |
| **Images par seconde** | **0,00** |

Le pipeline graphique n'est donc pas en cause au niveau de l'initialisation :
il est complet et opérationnel. Rien n'est présenté parce que rien n'est dessiné.

## 3. Attribution de l'arrêt

Trois mesures convergentes.

**La journalisation invitée cesse à 1,1 s.** Exactement 507 lignes, que le
processus tourne 60 s, 90 s ou 120 s, et quel que soit le niveau de journal. La
dernière ligne est une lecture réussie de 0x20000 octets à l'offset 13 893 632
de `DATA00.PAC`. L'invité termine un chargement de 14 Mo puis cesse tout appel
noyau.

**Les fils invités sont bloqués.** Sur 60 fils, 51 attendent sur un futex avec
0 ms de temps processeur cumulé. Seul le fil `Main XThread` travaille.

**Le processeur brûle côté hôte, pas sur les archives.** `rchar` croît de
130 Mio/s en continu, mais la position des descripteurs de `DATA00.PAC` et
`DATA01.PAC` ne bouge jamais. Le trafic est de 31 700 lectures par seconde à
3,9 ko de moyenne, dirigé vers `/dev/nvidia0` : une attente active du pilote
graphique, pas un chargement d'actifs.

**La pile du fil principal invité**, capturée sous `gdb` :

```
ac6PacDecoderDumpHook(...)::$_0::operator()(va=129)
  <- ac6PacDecoderDumpHook
  <- __imp__sub_821CC4D0
  <- __imp__sub_821D5EF8
  <- __imp__sub_821D7D90
  <- __imp__xstart
  <- rex::system::XThread::Execute
```

Sept trames seulement, et la racine est `xstart` : l'invité **n'a jamais quitté
son chemin de démarrage**. Il décode du PAC. Il n'existe aucune boucle de rendu
à ce stade — ce qui explique exactement les 26 dessins et les 0 images.

## 4. Deux leviers testés

**`async_shader_compilation=false` fait passer les dessins hôte de 0 à 1.** Le
générateur de pipelines compile les nuanciers de façon asynchrone et dessine
entre-temps avec un pipeline de remplacement ; la présentation de ces images est
alors **volontairement supprimée** par
`vulkan_async_skip_incomplete_frames`. Ce rejet n'est journalisé **qu'une seule
fois** (`static bool`), ce qui l'a rendu invisible jusqu'ici. Compiler
synchronement produit le premier dessin hôte réel.

**Découpler le vblank invité ne change rien.** AC6 force
`guest_vblank_sync_to_refresh=true` alors que le défaut du SDK est `false` ;
l'hypothèse d'un invité affamé de vblank était donc plausible. Mesure avec
`guest_vblank_sync_to_refresh=false` et `vsync=false` : **aucun changement**,
26/26/1 et 0,00 image par seconde. Hypothèse réfutée.

## 5. Résultat négatif important : les sondes PAC sont porteuses

La configuration contient 23 `[[midasm_hook]]`, dont huit nommées « Probe » ou
« Dump », vestiges apparents du travail de rétro-ingénierie du décodeur PAC.
Le fil principal ayant été capturé dans `ac6PacDecoderDumpHook`, leur retrait
semblait évident.

| Configuration | Résultat |
| --- | --- |
| 8 sondes retirées | **le jeu meurt**, `assert_always`, `xobject.cpp:373` |
| `ac6PacDecoderDumpHook` seule retirée | **le jeu meurt**, même assertion |
| toutes les sondes restaurées (contrôle) | **le jeu tourne** |

Le contrôle établit la causalité. **`ac6PacDecoderDumpHook` est porteuse malgré
son nom** : le chemin de décodage PAC en dépend. Elle ne peut pas être retirée
telle quelle, et la nommer « dump » est trompeur. Toute tentative future de
nettoyage des sondes doit partir de ce fait.

## 6. Ce qui n'est pas établi

- Une seule capture de pile a montré le fil principal dans cette accroche. Une
  observation unique sur un chemin chaud ne prouve pas qu'il y stagne ; elle
  prouve seulement qu'il y passe. Le caractère porteur de l'accroche, lui, est
  établi par le contrôle du §5.
- On ne sait pas encore si le décodage PAC **progresse lentement** ou **boucle**.
  C'est la question suivante, et elle est décidable : échantillonner la pile à
  plusieurs instants et comparer les trames sous l'accroche.
- L'écran capturé ne contient que le panneau de diagnostic. On ne peut pas
  conclure que la fenêtre de jeu est noire *parce que* rien n'est présenté
  plutôt que parce qu'elle n'est pas cartographiée ; le compteur d'images du
  panneau, lui, est sans ambiguïté à 0,00.

## 7. Prochaine tranche, dans l'ordre

1. Échantillonner la pile du `Main XThread` à 10 s, 30 s et 60 s et comparer :
   progression lente ou boucle. C'est la seule question qui commande la suite.
2. Si boucle : identifier la condition de sortie dans `sub_821CC4D0` et
   comparer au comportement de Xenia comme oracle borné.
3. Si progression lente : mesurer le coût par entrée décodée et chercher
   l'amplification, l'accroche porteuse étant la première suspecte.
4. Ne pas toucher aux sondes PAC sans un remplaçant fonctionnel.

`recompiler-generated` n'est pas `verified`. Zéro `REX_FATAL` reste acquis :
aucun des essais de ce cycle n'a réintroduit de piège.

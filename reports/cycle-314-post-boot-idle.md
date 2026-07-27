# AC6 cycle 314 — le jeu charge tout, puis attend

Le cycle 313 a rendu le runtime stable et vivant. Ce cycle caractérise ce qu'il
fait de ces 110 secondes, et écarte deux hypothèses.

## 1. Le profil d'exécution

| Mesure sur une exécution de 111 s | Valeur |
| --- | ---: |
| lectures `DATA00.PAC` | **47**, toutes entre 0,0 s et **0,4 s** |
| plage d'offsets lus | 3,4 Mo -> **2 055 Mo** |
| retours en arrière dans le fichier | **0** |
| présentations d'image (`XELOG_GPU PRESENT`) | **2** |
| dessins hôte | **2** |
| ticks audio périodiques | 42 |
| trames audio soumises | 10 |
| fils | **71** |

Le jeu lit donc **la totalité** de son archive principale en 0,4 seconde, de
façon strictement séquentielle, puis **ne relit plus rien** pendant 110
secondes. Le moteur audio continue de tourner, les fils vivent, mais plus aucune
image n'est produite après les deux premières.

Ce n'est pas un blocage dur : c'est une attente.

## 2. Hypothèse écartée — écran-titre attendant une entrée

Le pilote clavier/souris est initialisé (`MnK input driver initialized`) et une
fenêtre X réelle existe (`ac6recomp [rexglue-v0.8.0-RelWithDebInfo]`).

Envoi de `Return`, `space`, `KP_Enter` à la fenêtre via `xdotool`, avec activation
préalable :

| | moyenne des pixels | couleurs |
| --- | --- | --- |
| avant entrée | 0,023407 | 6 |
| après entrée | **0,023407** | **6** |

Aucun changement, au pixel près. Le jeu n'est pas à un écran-titre attendant une
validation, ou bien il n'interroge pas encore l'entrée. **Hypothèse écartée.**

## 3. Hypothèse écartée — service noyau non implémenté

Recherche dans le journal complet en niveau `debug` des motifs `unimplemented`,
`not implemented`, `stub`, `unsupported`, `missing`, `fail` : **aucune
occurrence** imputable au jeu. La seule correspondance concerne une couche
Vulkan de Mesa, sans rapport.

Le jeu n'attend donc pas derrière un service manquant qui se signalerait.
**Hypothèse écartée.**

## 4. Hypothèse écartée — notifications système non reçues

Le jeu enregistre **trois** écouteurs de notifications, masques `0x1`, `0x5` et
`0x6F`. Or `KernelState::RegisterNotifyListener` verrouille l'envoi initial sur
le **premier** écouteur seulement (`has_notified_startup_`) : les deux suivants
ne reçoivent jamais `XN_SYS_SIGNINCHANGED` ni `XN_SYS_INPUTDEVICESCHANGED`.

L'hypothèse était qu'une boucle principale attendait ces évènements sur un
écouteur tardif. Test avec `REX_NOTIFY_ALL_LISTENERS=1`, qui délivre l'ensemble
initial à tout écouteur abonné aux notifications système :

| | présentations | lectures | sortie |
| --- | ---: | ---: | --- |
| référence | 2 | 48 | 124 |
| toutes les notifications | **2** | **48** | 124 |

Strictement identique. **Hypothèse écartée.** Le correctif reste dans l'arbre,
désactivé par défaut et documenté comme expérience.

## 5. Ce que cela laisse

Le jeu a chargé ses données, initialisé son audio, présenté deux images, et
attend quelque chose qui n'est ni une entrée utilisateur, ni un service absent,
ni une notification système.

Pistes restantes, par ordre de coût :

1. **Comparer à Xenia** sur le même point : Xenia dépasse-t-il ce point avec le
   même XEX et les mêmes données ? C'est l'oracle borné prévu par le plan et il
   tranche entre « traduction fautive » et « attente légitime non satisfaite ».
2. **Échantillonner les piles des 71 fils** au repos, avec `SIGSEGV` passé, pour
   identifier sur quelle primitive le fil principal du jeu se bloque.
3. **Vérifier les autres entrées `[rexcrt]`** au-delà de la plage des assistants :
   le cycle 313 a montré qu'une seule ligne fausse pouvait tout bloquer, et seule
   la collision avec les assistants a été auditée, pas la justesse des adresses.

Aucun contenu de jeu n'est rendu. L'objectif « première mission jouable » reste
derrière cette attente.

`recompiler-generated` n'est pas `verified`.

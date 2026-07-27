# AC6 cycle 316 — c'est l'invité qui cesse de demander des images

Le cycle 315 a montré que l'oracle ne discriminait rien sous Xvfb. Ce cycle
attaque donc le problème par l'intérieur, sans affichage réel, et localise la
responsabilité de façon décisive.

## 1. Le GPU n'est pas le facteur limitant

Le runtime sélectionnait déjà le GPU NVIDIA (37 descripteurs `/dev/nvidia0`).
Forçage explicite du périphérique Vulkan pour lever le doute :

| | présentations | lectures |
| --- | ---: | ---: |
| sélection automatique | 2 | 48 |
| `--vulkan_device=0` (NVIDIA) | **2** | **48** |

Identique. Le choix de périphérique n'est pas en cause, et l'accélération
matérielle était déjà active. Xvfb n'est pas non plus le frein : la
surimpression ImGui est bien rendue et présentée par cette même chaîne.

## 2. Mesure décisive : `VdSwap`

`VdSwap` est l'appel par lequel l'invité demande la présentation d'une image.
Instrumentation d'un compteur à l'entrée :

| | valeur |
| --- | ---: |
| appels invités à `VdSwap` | **2** |
| présentations hôte (`XELOG_GPU PRESENT`) | **2** |

Les deux nombres sont **égaux**. Le chemin de présentation hôte honore donc
exactement ce que l'invité demande. **Il n'y a aucun défaut côté hôte.**

C'est l'invité qui cesse de demander des images après deux.

## 3. La synchronisation fonctionne

Restait l'hypothèse d'une boucle de rendu invitée privée de son signal.
Instrumentation de `GraphicsSystem::DispatchInterruptCallback` :

| | valeur |
| --- | ---: |
| interruptions livrées à l'invité | **plus de 4 000** |
| adresse du gestionnaire invité | `0x821E63B0` |
| interruptions perdues faute de gestionnaire | 3, au tout début |

Le vblank est donc bien cadencé, le gestionnaire invité est enregistré, et il
est appelé **plus de quatre mille fois** en 75 secondes — soit environ 55 Hz,
conforme aux 60 Hz annoncés par `[AC6-VBLANK] pacing`.

L'invité reçoit son signal de rendu 4 000 fois et n'en fait rien.

## 4. Ce que cela élimine

Quatre classes de causes sont écartées par mesure directe, cumulées avec le
cycle 314 :

| Classe | Statut |
| --- | --- |
| chemin de présentation hôte | **écarté** (§2, égalité exacte) |
| interruption / vblank / synchronisation | **écarté** (§3, 4 000 livraisons) |
| sélection ou accélération GPU | **écarté** (§1) |
| entrée utilisateur, service noyau absent, notifications | écarté au cycle 314 |

Il reste une cause de nature **logique** : la machine à états du jeu n'atteint
pas l'état où elle dessine. Elle tourne — 26 s de processeur sur un fil au
cycle 314 — mais dans une phase antérieure au rendu.

## 5. Indice à suivre : le profil de lecture

Les 48 lectures couvrent des offsets allant jusqu'à 2 055 Mo pour un total
d'environ 12 Mo seulement. Ce n'est pas un chargement d'actifs : c'est un
parcours de table des matières. Le jeu a lu son index, puis s'est arrêté avant
de charger le contenu.

Toutes les lectures rendent `status=0x103`, soit `STATUS_PENDING`, avec un
`iosb_info` renseigné. La piste la plus prometteuse est donc l'achèvement des
entrées/sorties asynchrones : si le jeu attend une complétion qui n'est jamais
signalée, il tourne indéfiniment sans jamais passer à l'étape suivante, ce qui
correspond exactement à l'ensemble des observations.

## 6. Prochaine tranche

1. Vérifier l'achèvement asynchrone : chaque `NtReadFile` rendant
   `STATUS_PENDING` voit-il son évènement signalé et son APC délivrée ?
   `KernelState` porte un mécanisme de report (`kDeferredOverlappedDelayMillis`)
   qu'il faut instrumenter.
2. Si les complétions sont saines, instrumenter le gestionnaire invité
   `0x821E63B0` par accroche `midasm` et suivre la branche qu'il prend : il
   décide, 4 000 fois, de ne pas dessiner.
3. Auditer les adresses `[rexcrt]` restantes.

Aucun contenu de jeu n'est rendu ; l'objectif reste derrière cette phase.

`recompiler-generated` n'est pas `verified`.

## 7. Addendum : l'achèvement par APC est écarté

La piste du §5 — une complétion d'entrée/sortie asynchrone jamais signalée —
est réfutée par mesure directe. Compteurs posés aux deux extrémités du
mécanisme :

| | valeur |
| --- | ---: |
| `XThread::EnqueueApc` | **0** |
| `XThread::DeliverAPCs` | 3 |

**Aucune APC n'est jamais mise en file.** Le jeu ne passe pas de routine APC à
`NtReadFile` : le chemin `apc_requested` n'est jamais emprunté. Il s'appuie donc
sur l'évènement, que le code positionne bien (`signal_event = true`), ou sur la
lecture directe du bloc d'état.

L'attente du jeu ne vient pas d'une complétion asynchrone perdue.
**Hypothèse écartée.**

Le bilan des causes éliminées par mesure s'établit donc à six : chemin de
présentation hôte, interruption/vblank, sélection GPU, entrée utilisateur,
service noyau absent, notifications système, et à présent complétion par APC.

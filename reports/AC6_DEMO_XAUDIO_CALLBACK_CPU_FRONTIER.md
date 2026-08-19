# AC6 démo PAL — frontière CPU du callback XAudio

Date : 2026-08-19  
Cible : démo PAL Xbox 360, chemin XAudio du runtime natif

## Verdict

La correction d'affinité par thread est déjà partiellement intégrée dans le
runtime actuel :

```text
KeSetAffinityThread
→ pin_guest_thread_processor(object, one_hot_mask)
→ GuestThread.processor
→ publication à r13+0x10C lorsque la fiber s'exécute
```

Le rapport antérieur qui décrivait l'affinité comme entièrement oubliée est
donc obsolète sur ce point.

La frontière restante est plus étroite : `dispatch_xaudio_frame` ne s'exécute
pas dans une `GuestThread`. Il copie le contexte primaire, change seulement
`current_guest_thread_id`, puis appelle le callback sans publier de processeur
Xenon dans le PCR copié.

```text
fibers ordinaires            : identité CPU publiée
callback XAudio synthétique  : identité CPU non publiée
```

## État PAL joint

La table de l'objet audio global contient deux pointeurs par processeur :

| CPU | A | B |
|---:|---:|---:|
| 0 | `global+0x0C` | `global+0x10` |
| 1 | `global+0x14` | `global+0x18` |
| 2 | `global+0x1C` | `global+0x20` |
| 3 | `global+0x24` | `global+0x28` |
| 4 | `global+0x2C` | `global+0x30` |
| 5 | `global+0x34` | `global+0x38` |

La trace qualifiée du premier callback donne :

```text
CPU 0..3 : paires nulles
CPU 4..5 : paires construites
```

La même phase, au tick 106, épingle trois nouveaux objets thread avec les
masques :

```text
0x10
0x10
0x20
```

soit les processeurs 4, 4 et 5.

Cela prouve que CPU 0 est incorrect. Cela ne distingue pas encore CPU 4 de
CPU 5 pour le callback render-driver.

## Contrat ajouté

Le nouveau contrat C++ :

```text
include/ac6demo/xaudio_callback_cpu_contract.hpp
```

encode :

1. les six paires et leurs offsets exacts ;
2. la validation d'un processeur explicite ;
3. le refus d'un descripteur nul ;
4. l'inférence seulement lorsqu'une unique paire complète existe ;
5. le refus de l'état PAL ambigu où CPU 4 et CPU 5 sont tous deux actifs.

Le runtime ne reçoit donc aucun `processor = 4` caché sous un commentaire
optimiste. L'expérience devra demander explicitement 4 ou 5.

## A/B suivant

Deux routes process-fresh doivent être comparées :

```text
AC6_DEMO_EXPERIMENTAL_XAUDIO_PROCESSOR=4
AC6_DEMO_EXPERIMENTAL_XAUDIO_PROCESSOR=5
```

Autour du callback :

```text
sauver [r13+0x10C]
écrire 4 ou 5
journaliser la paire A/B
appeler le callback
restaurer le byte PCR
```

La promotion exige simultanément :

```text
descripteur choisi non nul
→ retour du callback
→ XAudioSubmitRenderDriverFrame atteint
→ frame 0x1800 validée
→ événement worker publié
→ worker audio réveillé
→ nouvelle frame soumise
```

Le seul fait « le trap disparaît » ne suffit pas. Un mauvais CPU peut déplacer
la faute plus loin avec une ponctualité admirable.

## Correction secondaire

`KeSetAffinityThread` conserve maintenant l'identité du processeur dans
`GuestThread`, mais son out-paramètre de valeur précédente reste simplifié à
`1`. La fonction PAL convertit cette valeur avec `cntlzw/subfic`, ce qui
justifie un front séparé sur le masque précédent. Ce défaut n'empêche pas le
nouveau contrat de sélectionner explicitement CPU 4 ou 5, mais il reste une
inexactitude ABI.

## État

| Front | Verdict |
|---|---|
| affinité one-hot stockée par thread | fermé dans le code actuel |
| PCR republié à chaque reprise de fiber | fermé dans le code actuel |
| CPU 0 incorrect pour le callback XAudio | fermé A- |
| candidats PAL CPU 4 et 5 | fermé A- |
| choix implicite entre 4 et 5 | refusé |
| contrat C++ explicite / unique | ajouté et testé |
| A/B process-fresh 4 contre 5 | ouvert |
| sortie audio audible | hors de cette passe |

# AC6 démo PAL — callback D3D transporté par scratch + PM4 interrupt

Date : 2026-08-19  
Cible : démo PAL Xbox 360, image mémoire à `0x82000000`  
Image SHA-256 : `b81941994944b84f5792fd7b89cd698ca429b13c1bb4f501ea12e49dc54c2f01`

## Verdict

La frontière n'est pas un appel PPC exécuté par le GPU. D3D encode un callback
CPU et son paramètre dans les scratch registers Xenos, puis émet un paquet
`PM4_INTERRUPT`. Le gestionnaire d'interruption CPU recharge les deux valeurs
et appelle le callback.

```text
D3D::CDevice
→ encoder 0x821BA1F8
→ scratch4 = callback PPC
→ scratch5 = paramètre
→ WAIT_REG_MEM
→ PM4_INTERRUPT(cpu_mask)
→ callback graphique enregistré 0x821B9710, source 1
→ callback(parameter)
```

Cette chaîne explique pourquoi l'adresse `0x821C4A60` apparaît dans un command
buffer sans devenir du code GPU. Le GPU transporte la demande vers le CPU ; il
ne développe pas soudain une passion pour PowerPC.

## Encoder `0x821BA1F8`

Arguments minimaux :

```text
r3  CDevice
r4  curseur de command buffer
r5  flags d'encodage
r6  callback CPU
r7  paramètre du callback
```

Le cœur du stream est :

```text
TYPE0 base 0x057C, count 2
    scratch4 = r6
    scratch5 = r7

TYPE0 base 0x0578, count 1
    scratch0 = cpu_mask

PM4_WAIT_REG_MEM
    attend la publication du scratch

PM4_INTERRUPT (opcode 0x54)
    payload = cpu_mask
```

Le wrapper `0x821BAA78` convertit ses arguments `r5/r6` en `r6/r7` avant
d'appeler l'encoder.

## Producteurs statiques qualifiés

| Callsite | Route | Callback | Paramètre minimal |
|---:|---|---:|---|
| `0x821B9120` | wrapper `0x821BAA78` | `0x821C4A60` | paramètre de worker calculé |
| `0x821C4D30` | direct `0x821BA1F8` | `0x821C4A60` | paramètre de worker calculé |
| `0x821C5458` | direct `0x821BA1F8` | `0x821C5190` | état callback du device |
| `0x822E4444` | tail-wrapper | `0x822E4240` | argument entrant |
| `0x822E4470` | wrapper | `0x822E4268` | argument entrant |

`0x821C4A60` est donc bien un callback de démarrage/gestion de worker D3D,
mais l'ancienne formulation « StartWorkerQueue est un paquet GPU » est fausse.
C'est un callback CPU **référencé par** un paquet GPU.

## Dispatcher CPU `0x821B9710`

Lorsque `source == 1`, la fonction :

```text
state    = [device+0x2A94]
callback = [state+0x10]
argument = [state+0x14]
if callback != 0:
    callback(argument)

cpu = *(u8 *)(r13+0x10C)
[state+0] &= ~(1 << cpu)
```

Le byte PCR processeur intervient donc également dans l'acquittement du worker.
La correction d'affinité publiée sur `main` reste pertinente, mais elle ne
suffit pas à garantir l'ordre correct de livraison du callback.

## Écart du modèle natif actuel

Le processeur PM4 natif :

```text
accepte uniquement cpu_mask == 0x4
réduit ce masque à cpu = 2
met l'interruption en attente
la livre au prochain tour externe du scheduler
```

Le contrat Xenos représenté par Xenia est plus large :

```text
cpu_mask non nul
bits autorisés 0..5
une livraison source=1 pour chaque bit positionné
```

L'écart de largeur est certain. L'impact temporel de la livraison différée
reste une hypothèse forte mais non démontrée : Xenia livre `PM4_INTERRUPT`
pendant le traitement du packet, alors que le bridge AC6 attend le tick externe
suivant. Si D3D enfile `StartWorkerQueue` avant la fin de son bootstrap, ce
décalage peut laisser la file dans un état que le hardware n'observe jamais.

## Code ajouté

La passe ajoute :

```text
include/ac6demo/xenos_cpu_interrupt_contract.hpp
tests/xenos_cpu_interrupt_contract_tests.cpp
tools/run_xenos_cpu_interrupt_contract_test.sh
tools/verify_ac6_pal_d3d_callback_bridge.py
```

Le contrat C++ :

- accepte les masques non nuls limités aux six CPUs ;
- produit une requête typée par bit, dans l'ordre CPU 0..5 ;
- conserve `source=1`, le masque brut, `scratch4` et `scratch5` ;
- refuse les masques zéro ou hors plage.

Il n'est pas encore branché dans le runtime par défaut. Le faire sans capture
A/B du moment de livraison transformerait une correction de structure en pari
de scheduling, activité pour laquelle les logiciels concurrents n'ont pas
besoin d'encouragement.

## Expérience suivante

Ajouter une trace bornée au point où le PM4 parser rencontre l'opcode `0x54` :

```text
tick
ring / indirect-buffer offset
cpu_mask
scratch update mask
scratch base
scratch4 callback
scratch5 parameter
```

Puis à la livraison :

```text
tick
source
cpu
callback graphique externe
contexte device
callback scratch effectivement lu par 0x821B9710
paramètre
```

Comparer ensuite :

```text
A  livraison différée actuelle
B  livraison source=1 dans la même slice, derrière un opt-in
```

Le critère n'est pas « moins de crash ». Il faut observer :

```text
0x821C4A60 atteint
worker events publiés
command buffer consommé
nouveau write pointer du ring
```

## Correction annexe

Le packet `0xC0025800` de `0x821ADD90` utilise l'opcode `0x58`, nommé
`EVENT_WRITE_SHD`. `EVENT_WRITE_EXT` est `0x5A`. Cela ne change pas la
conclusion précédente : ce chemin reste un bracket de mesure facultatif.

## Audit adversarial

- La présence du callback `0x821C4A60` dans un buffer ne prouve pas qu'il est
  atteint sur la route courante.
- Le masque observé `0x4` qualifie CPU 2 pour ce corpus, pas une règle générale.
- Une interruption multi-bit doit produire plusieurs livraisons, pas choisir
  arbitrairement le premier bit.
- La livraison same-slice est conforme au modèle Xenia, mais son effet AC6 doit
  encore être mesuré avant activation par défaut.
- `StartWorkerQueue` et `0x821C5190` utilisent le même transport avec des rôles
  différents ; ils ne doivent pas être confondus.

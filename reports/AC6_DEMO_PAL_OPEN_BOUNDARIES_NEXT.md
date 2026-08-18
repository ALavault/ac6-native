# AC6 démo PAL — frontières suivantes après fermeture renderer/XAudio

## 1. Dispatcher du registre de callbacks, catégorie 2

But : retrouver le producteur qui livre :

```text
callback 0x821ADAB8
event 17
channel 6
```

Capture minimale :

```text
break 0x821ADAB8
r3, r4, LR, pile 0x100, tick
```

La capture ferme à la fois :

- l'owner concret du service 47 ;
- la condition d'activation du renderer ;
- le moment exact où la file de packets devient valide.

## 2. CPU du callback XAudio

Implémenter d'abord un traceur, pas une correction forcée :

```text
GuestThread.affinity_mask
processor dérivé
r13+0x10C
descripteurs A/B
r5
```

L'expérience stateful est acceptable seulement si :

- masque one-hot ;
- CPU inférieur à 6 ;
- écriture du PCR bornée au thread ou callback ;
- restauration garantie ;
- trap avant appel si le descripteur est nul.

## 3. Après ces deux discriminants

### Audio

```text
vslot décodeur +0x18 réussit
→ worker audio réveillé
→ sortie vers frame six canaux de 0x1800 octets
→ XMA slots non nuls
```

### Rendu

```text
(17,6)
→ gate 0x5460
→ packet builder 0x821ADD90
→ packet processor 0x821BD970
→ enqueue 0x821C4A60
→ KeSetEvent worker
→ nouveau ring write
```

## Priorité

1. CPU XAudio, car le défaut natif est local et explicite ;
2. dispatcher `(17,6)`, car il ferme l'activation renderer sans forcer la file ;
3. première sortie audio ou premier packet renderer, selon la première chaîne
   qui progresse sous le bridge corrigé.

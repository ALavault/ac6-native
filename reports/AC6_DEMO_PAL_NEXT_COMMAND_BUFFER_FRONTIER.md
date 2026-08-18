# AC6 démo PAL — frontière renderer révisée

## Point de départ

La porte `device+0x5460` est un enable de monitoring, pas l'enable global du
renderer. La fonction par trame poursuit son travail à `0x821C5920` lorsque ce
champ vaut zéro.

## Frontière suivante

```text
0x821C5920
→ construction réelle de commandes
→ fermeture du buffer
→ publication à la file
→ consommation worker
→ ring Xenos
```

## Capture minimale

Pour le premier tick après START où le frontend a produit du travail :

```text
à 0x821C59AC :
    device
    argument source
    command write cursor avant/après

à 0x821C5C30 :
    buffer
    taille
    owner
    résultat

au premier changement de producer/consumer :
    thread
    LR
    ancien/nouveau index
    base du buffer

à l'écriture ring :
    wptr
    indirect buffer address
    dword count
```

## Refus

Ne pas :

- forcer `device+0x5460 = 1` ;
- injecter l'événement de certification `(17,6)` ;
- réveiller les workers sans buffer ;
- attribuer la table de factory de `0x820A45E0` au protocole graphique.

Ces interventions modifieraient de la télémétrie ou inventeraient une cause
sans réparer la publication des commandes.

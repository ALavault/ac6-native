# AC6 cycle 317 — sources d'interruption, et une fausse piste MMIO (corrigée au §5)

Suite directe du cycle 316 : l'invité reçoit 4 000 interruptions et ne dessine
pas. Ce cycle lit le gestionnaire invité et trouve pourquoi.

## 1. Les deux sources d'interruption ne sont pas équivalentes

Compteurs **par source** posés dans `DispatchInterruptCallback` (le cycle 316
utilisait un compteur unique, qui ne pouvait pas les distinguer) :

| Source | Nature | Occurrences en 75 s |
| ---: | --- | ---: |
| 0 | vblank | **4 250** |
| 1 | fin de traitement du flux de commandes (EOP) | **4** |

Le gestionnaire invité `sub_821E63B0` commence par :

```c
// cmplwi cr6,r3,1      ; r3 = source
if (!ctx.cr6.eq) goto loc_821E6440;   // source != 1 -> autre chemin
```

La branche « travail » du gestionnaire est donc réservée à la **source 1**, qui
n'est survenue que **quatre** fois avant de cesser. Les 4 250 vblanks empruntent
l'autre chemin.

## 2. Le chemin vblank est conditionné par un registre MMIO

`loc_821E6440`, emprunté par les 4 250 vblanks :

```c
loc_821E6440:
    if (r3 != 0) goto loc_821E6460;              // ni 0 ni 1 -> sortie
    r11 = PPC_MM_LOAD_U32(0x7FC86544);           // lecture MMIO
    r11 = r11 & 1;                                // bit 0
    if (r11 == 0) goto loc_821E6460;             // bit nul -> sortie immédiate
    r3 = r30;
    sub_821EFBA0(ctx, base);                     // <- le vrai travail vblank
loc_821E6460:
    return;
```

Le travail de vblank n'est exécuté **que si le bit 0 du registre MMIO
`0x7FC86544` est à 1**. C'est une lecture `PPC_MM_LOAD_U32`, donc un registre
matériel émulé, pas de la mémoire ordinaire.

Si ce bit reste nul, le gestionnaire sort immédiatement — 4 250 fois — et
`sub_821EFBA0` n'est jamais appelé. Ce qui correspond exactement à l'observation
du cycle 316 : l'invité reçoit son signal et n'en fait rien.

## 3. Hypothèse à vérifier

Deux causes possibles, toutes deux vérifiables :

1. **Le registre n'est pas implémenté** et rend systématiquement 0. Il faut
   identifier `0x7FC86544` dans la table MMIO du SDK et voir ce qu'il rend.
2. **Le bit est légitimement nul** parce que l'invité ne l'a pas armé, faute
   d'être passé par une initialisation antérieure — auquel cas la cause est
   encore en amont.

La distinction est mécanique : instrumenter la lecture MMIO à cette adresse et
relever la valeur rendue ainsi que son origine.

## 4. Statut

Le blocage est désormais réduit à **une lecture de registre précise** et à une
condition binaire, au lieu d'un « quelque part dans la machine à états ». C'est
la localisation la plus fine obtenue depuis le cycle 308.

Causes éliminées par mesure à ce stade : chemin de présentation hôte,
livraison des interruptions, sélection GPU, entrée utilisateur, service noyau
absent, notifications système, complétion par APC.

`recompiler-generated` n'est pas `verified`.

## 5. CORRECTION : le verrou MMIO n'existe pas

Le §2 et le titre d'origine affirmaient que le rendu était bloqué parce que le
bit 0 du registre `0x7FC86544` restait nul. **C'est faux**, et l'erreur vient
d'avoir déduit l'état du registre au lieu de le mesurer.

Le registre est implémenté dans le SDK :

```c
case 0x1951:    // interrupt status
  return 1;     // vblank
```

`0x7FC86544` est un registre GPU Xenos, plage `0x7FC80000`-`0x7FCFFFFF`, indice
`0x6544 / 4 = 0x1951`. Il rend **1**. Le bit 0 est donc **toujours positionné**,
la condition est **toujours vraie**, et `sub_821EFBA0` est appelé.

Vérification directe, accroche posée à l'entrée de `sub_821EFBA0` :

| | valeur |
| --- | ---: |
| interruptions vblank (source 0) | **4 250** |
| appels à `sub_821EFBA0` | **4 250** |
| appels à `VdSwap` | 2 |

Les deux premiers nombres sont **égaux**. Le travail de vblank de l'invité
s'exécute à **chaque** interruption, sans exception.

### Ce que cela change

Le blocage n'est ni dans la livraison des interruptions, ni dans un registre
manquant, ni dans un verrou matériel. **L'invité exécute sa boucle de trame
4 250 fois et n'y demande pas de présentation.** La cause est dans la logique
de `sub_821EFBA0` ou dans l'état de jeu qu'elle pilote : la machine à états
tourne, dans un mode qui ne dessine pas.

### Leçon

Le §2 lisait correctement le code désassemblé mais **supposait** la valeur du
registre. Une supposition sur un état d'exécution doit être mesurée avant d'être
publiée : la vérification coûtait une accroche et une exécution de 75 secondes.
C'est la seconde fois dans cette campagne qu'une conclusion tirée du code seul
est renversée par la mesure — la première étant `0x821CCBE0` au cycle 312.


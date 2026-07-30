# Cycle 322 — la valeur partagée était lue sur la mauvaise moitié

> **[CONCLUSION CAUSALE RÉFUTÉE PAR LE CYCLE 323 — À LIRE EN PREMIER]**
> Ce qui reste valide, et qui était le vrai apport de ce cycle : le protocole
> `sub_82346108`, l'arithmétique d'adresse `0x82870780 + 152 / + 168`, le fait
> que le champ est un mot de **64 bits** dont la moitié basse est à
> `0x8287082C`, et la règle « toute mesure invité déclare sa largeur et sa
> moitié ». Le cycle 323 confirme tout cela et applique la règle.
>
> Ce qui est **faux** : la conclusion que « l'anomalie n'existe pas » et que
> « la valeur n'est ni `0` ni `1` ». Ce cycle a corrigé une ambiguïté de mesure
> puis **supposé** la valeur au lieu de la mesurer — exactement l'erreur que le
> cycle 317 §5 s'était déjà infligée et documentée comme leçon.
> Mesure du cycle 323, hors débogueur, sur le fichier de mémoire invité :
> `0x82870828..0x8287082F = 0000000000000000`, soit `0` sur les **deux**
> moitiés. `sub_8233A730` initialise d'ailleurs ce champ à `0` par `std`.
> L'anomalie du cycle 320 est donc réelle, et sa cause est que le thread qui
> pose le signal peut le reconsommer lui-même. Voir
> `reports/cycle-323-self-consumed-wake-and-contamination-sweep.md`.

## Cible

- Produit : AC6 Xbox 360 PAL, Xenon PPC **big-endian**, Xenos
- Module : `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Base image : `0x82000000`
- Route : `deterministic-fast-path` — analyse statique, aucune exécution

## Résultat

Le cycle 320 a fondé toute sa ligne d'enquête sur une anomalie : le thread
principal attendait la valeur `0`, la valeur partagée *valait* `0`, et pourtant
il restait endormi. C'est cette anomalie qui a motivé l'hypothèse du vol de
signal, réfutée au cycle 321.

**L'anomalie n'existe pas. La mesure portait sur la mauvaise moitié d'un mot
de 64 bits.**

## Le protocole, lu intégralement

`sub_82346108(obj, wanted)`, sortie recompilée, preuve de flot de contrôle :

```text
obj+0  : handle de signal (mutex 0xF80000A4)
obj+4  : handle d'attente (event 0xF80000A8)
obj+16 : valeur courante

boucle : acquérir le mutex (sub_821F40E8, timeout -1)
         ld    r11, 16(r31)          <- chargement 64 bits
         cmpld cr6, r11, r30         <- comparaison 64 bits non signée
         si différent -> SignalAndWait(mutex, event) puis recommencer
         sinon relâcher le mutex (sub_823910F0) et rendre la main
```

C'est une variable de condition invité canonique. Les deux appelants sont des
thunks :

| appelant | objet | valeur attendue |
|---|---|---:|
| `sub_8233AB00` (thread principal) | `arg + 152` | **0** |
| `sub_8233AD70` (worker) | `arg + 152` | **1** |

## L'arithmétique d'adresse

`sub_8233A730` initialise la structure :

```text
lis  r11,-32121
addi r3,r11,1920        ->  r3 = 0x82870780
std  r11,168(r3)        ->  écrit 8 octets à 0x82870828
```

Donc :

```text
objet passé au wait = 0x82870780 + 152 = 0x82870818
valeur  (obj+16)    = 0x82870780 + 168 = 0x82870828
```

`0x82870828` est exactement l'adresse relevée au cycle 320. C'est bien le même
objet ; l'identification n'est pas en cause.

## L'erreur

`std` écrit **huit** octets, `0x82870828..0x8287082F`. Xenon est big-endian :
l'octet de poids fort est à l'adresse la plus basse. Donc

```text
mot 32 bits à 0x82870828  =  MOITIÉ HAUTE du compteur
mot 32 bits à 0x8287082C  =  MOITIÉ BASSE du compteur
```

Le cycle 320 a rapporté « the shared value at Xbox 360 PAL address
`0x82870828` was `0` ». Pour **toute** valeur 64 bits inférieure à `2^32`, la
moitié haute vaut `0`. Cette lecture ne peut donc pas distinguer `0` de `1`,
`2`, `3`, … : elle est vraie et sans information.

L'anomalie « waiter éligible mais endormi » est un artefact de cette lecture.

## Ce que cela change

Une fois l'artefact retiré, l'observation du cycle 320 devient parfaitement
ordinaire et **cohérente avec un protocole qui fonctionne** : la valeur n'est
ni `0` ni `1`, donc *aucun* des deux waiters n'est éligible, et les deux
dorment. Il n'y a jamais eu de waiter éligible négligé.

Cela explique aussi, rétrospectivement, pourquoi la régression du cycle 321 n'a
trouvé aucune famine : il n'y avait pas de problème de livraison de signal à
trouver.

Les trois cycles 320, 321 et 322 se referment donc ainsi :

- 320 : observation dynamique correcte, interprétation causale fausse ;
- 321 : hypothèse de vol de signal réfutée par régression ;
- 322 : l'anomalie qui les motivait est une erreur de lecture d'endianness.

Confiance : `confirmed` pour le protocole, l'arithmétique d'adresse et
l'ambiguïté de la mesure — tout est lisible dans la sortie recompilée et dans
l'encodage big-endian. **Aucune revendication n'est faite ici sur la valeur
réelle** : elle exige une mesure.

## Front suivant, et il tient en un mot de 32 bits

La question n'est plus « pourquoi un waiter éligible dort-il », mais :

1. relever la valeur 64 bits à `0x82870828`, ou simplement le mot bas à
   **`0x8287082C`**, au moment du gel. Une seule lecture tranche entre
   « valeur `0` ou `1`, donc vrai défaut de synchronisation » et
   « valeur `>= 2`, donc les deux waiters attendent légitimement un producteur
   qui ne progresse plus » ;
2. si la valeur est `>= 2`, identifier le producteur censé la ramener. Les
   fonctions qui touchent la structure `0x82870780` sont
   `sub_8233A730` (initialisation), `rex_sub_8233D0B0`, `sub_8234B408`,
   `rex_sub_823344D8`, `sub_823D4A28` et `sub_823D7800`.

Toute future mesure d'une valeur invité doit déclarer sa largeur et sa moitié.

## Défaut adjacent relevé, non corrigé

`sub_821F5828` contient un
`REX_FATAL("Unresolved branch from 0x821F5880 to 0x821F5854")` sur le chemin
de reprise alertable, atteint quand `alertable != 0` et que le statut vaut
`257` (`X_STATUS_USER_APC`). Le chemin de la boucle de trame passe
`alertable = 0` et ne l'atteint pas, mais c'est une branche non résolue de la
même famille que le cycle 304, sur une fonction de synchronisation centrale.

## Modifications

Aucune. Tranche d'analyse statique.

## Validation exécutée

```text
protocole sub_82346108 extrait de la sortie recompilée          lecture directe
thunks sub_8233AB00 / sub_8233AD70, valeurs attendues 0 et 1    lecture directe
arithmétique 0x82870780 + 152 / + 168                           recalculée
correspondance avec l'adresse du cycle 320                      identique
```

Ni preuve de jouabilité, ni preuve de parité retail.

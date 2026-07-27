# AC6 cycle 309 — la frontière de démarrage avance, et le cycle 308 est corrigé

Objectif de campagne : un exécutable Linux permettant de jouer la première
mission. Ce cycle corrige une erreur d'attribution du cycle 308, fait avancer la
frontière de démarrage invitée, et localise précisément le blocage suivant.

## 1. Correction : l'erreur de mesure du cycle 308

Le cycle 308 a capturé la pile du fil principal invité sous `gdb` et l'a trouvée
dans `ac6PacDecoderDumpHook`. J'en avais conclu que l'invité stagnait dans le
décodage PAC.

**Cette pile était prise sur un arrêt `SIGSEGV`, pas sur l'interruption
demandée.** ReXGlue installe un gestionnaire `SIGSEGV` pour réaliser la MMIO
invitée et la protection de pages : un `SIGSEGV` est donc le fonctionnement
*normal*, pas une faute. Sans `handle SIGSEGV nostop noprint pass`, `gdb`
s'arrête au premier accès mémoire invité et la pile obtenue décrit un piège
mémoire, pas un blocage.

`tools/ac6-stall-bt.py` est corrigé en conséquence, et `tools/ac6-sample-bt.py`
est ajouté pour échantillonner à plusieurs instants.

Conséquence pratique : passer les `SIGSEGV` rend l'exécution sous `gdb` si lente
qu'aucun échantillon n'est obtenu en dix minutes. Les piles ne sont exploitables
que sur un arrêt *terminal* (abort), atteint ici en 1,4 s.

## 2. Correction : la sonde PAC n'est pas porteuse

Le cycle 308 concluait que `ac6PacDecoderDumpHook` était porteuse, parce que son
retrait tuait le jeu. C'est l'inverse.

| Configuration | Lignes de journal | Fin | Issue |
| --- | ---: | --- | --- |
| avec la sonde | 507 | 1,08 s | silence, aucune image, le processus survit |
| **sans la sonde** | **569** | **1,43 s** | assertion sur un blocage **nouveau** |

Le retrait fait **avancer** l'invité de 62 lignes de journal et de 0,35 s. Le
jeu ne meurt pas d'avoir perdu une fonction : il meurt d'être arrivé plus loin,
sur un obstacle que la stagnation précédente masquait. Vérification du code : la
sonde ne modifie **aucun** registre invité — elle ne fait que lire.

Le retrait de la sonde est donc un **gain**, pas une régression, et le cycle 308
doit être lu avec cette correction.

## 3. Le blocage suivant, localisé et déterministe

Chaîne d'appel complète, obtenue à l'abort :

```
__imp__sub_821F7FC8
  -> __imp__sub_821D4ED0
     -> RtlEnterCriticalSection_entry(cs = 0x826A19B0)
        -> xeKeWaitForSingleObject(object_ptr = 0x826A19B0, wait_reason = 8)
           -> XObject::GetNativeObject(as_type = -1 -> 130)
              -> assert_always()   xobject.cpp
```

`GetNativeObject` lit le type dans l'en-tête de répartition invité. Les types
implémentés sont 0 et 1 (évènement), 2 (mutant), 5 (sémaphore) ; tout le reste
déclenche l'assertion. Ici le type vaut **130**.

Contenu réel de la mémoire invitée en `0x826A19B0`, en gros-boutiste :

```
0x826A19B0:  82 06 79 E0   00 00 00 00   00 00 00 01   A3 48 00 00
             ^^^^^^^^^^^
             0x820679E0 -- un pointeur invité, pas un en-tête
```

Le premier mot est un pointeur `0x820679E0`. L'octet de poids fort d'un pointeur
invité valant toujours `0x82`, le champ `type` lu vaut mécaniquement 130. **Ce
n'est pas une section critique**, ou elle n'a jamais été initialisée.

Ce n'est donc **pas** un type d'objet noyau manquant à implémenter : c'est un
mauvais pointeur. Implémenter les types 3, 4, 6-9 et 18-24 ne corrigerait rien
ici.

Déterminisme vérifié : **4 exécutions sur 4** donnent 569 lignes, la même
adresse `0x826A19B0` et le même type 130. Aucune course.

## 4. État courant

- Corpus : **0 `REX_FATAL`**, 48 unités, inchangé depuis le cycle 307.
- Frontière de démarrage invitée : **1,43 s**, contre 1,08 s au cycle 308.
- Aucune image rendue à ce stade ; la question de l'affichage reste derrière
  celle du démarrage.

## 4 bis. Piège de méthode : les registres ne sont pas par trame

Tentative de remonter à l'origine du pointeur en lisant les registres invités
dans les trames `sub_821D4ED0` puis `sub_821F7FC8` : les deux renvoient
**exactement** les mêmes valeurs.

C'est attendu. ReXGlue fait circuler un **unique `PPCContext&`** à travers toutes
les trames invitées ; `frame N` puis `p ctx.rX` lit donc l'état *courant* du
contexte, pas celui de la trame choisie. Les valeurs observées le confirment :
`r28 = 0x826A19B0` alors que `r30 = 0`, ce qui est incompatible avec
`r28 = r30 + 16` établi en tête de fonction — ces deux lectures ne datent pas du
même instant.

**Il est donc impossible de reconstituer les arguments d'un appel invité en
lisant les registres depuis une trame de pile.** Il faut un point d'arrêt au
site d'appel, ou une accroche qui journalise les registres au moment voulu.
Toute analyse antérieure fondée sur des registres lus par trame est à rejeter.

## 4 ter. L'indice passé est aberrant, et l'appel est indirect

Trois faits mesurés resserrent le diagnostic.

**Un seul cas dans toute l'exécution.** Une trace ajoutée dans
`RtlEnterCriticalSection_entry` signale toute section critique dont le type
d'en-tête sort de {0,1,2,5}. Sur un démarrage complet : **une seule**
occurrence, celle-ci. Tous les autres verrous du démarrage sont sains. Ses
champs confirment que ce n'est pas une section critique :
`lock_count=16846848`, `recursion=-1555562476`, `owner=0xA3480018`.

**L'indice n'est PAS absurde — voir le cycle 310, qui corrige ce paragraphe.**
J'avais résolu `r3 * 152 + 0x829E64B8 = 0x826A19B0` modulo 2^32 et obtenu
`r3 = 56 490 181`, concluant à un `r3` parasite. La mesure directe (cycle 310)
donne `r3 ∈ {0,1,2,3}`. Le calcul était juste, sa prémisse était fausse :
il supposait que l'adresse fautive *provenait* de ce calcul.

**L'appel est indirect.** `sub_821F7FC8` ne contient **aucun** appel direct à
`sub_821D4ED0`. La liaison passe donc par un pointeur de fonction ou une table
virtuelle. Cela rejoint le premier mot lu en `0x826A19B0`, `0x820679E0`, qui a
la forme d'un pointeur de table virtuelle.

L'hypothèse à tester en premier est donc une **répartition indirecte erronée** :
l'appel n'aboutit pas forcément à la fonction voulue, auquel cas `r3` n'est pas
un indice mais l'argument d'une autre fonction. Les 205 `Unresolved call`
éliminés au cycle 307 concernaient précisément des cibles indirectes ; cette
piste est à privilégier avant toute hypothèse de corruption mémoire.

## 5. Prochaine tranche

1. Remonter à l'origine de `0x826A19B0` en posant un point d'arrêt **au site
   d'appel**, à l'entrée de `sub_821D4ED0`, et non en lisant les registres
   depuis une trame (§4 bis). Le code traduit calcule la section critique comme
   `r3_entrée * 152 + 0x829E64A8 + 16` : un tableau de structures de 152 octets.
   Il faut donc établir la valeur de `r3` à l'entrée, et si elle est aberrante,
   remonter à `sub_821F7FC8` qui la fournit.
2. Comparer à Xenia comme oracle borné sur le même point d'entrée : si Xenia
   passe, la faute est dans la traduction ; sinon, dans l'initialisation invitée.
3. Ne pas implémenter de nouveaux types d'objets noyau tant que le §3 n'est pas
   tranché : la mesure dit que ce n'est pas le manque.

`recompiler-generated` n'est pas `verified`.

# L'horloge du guest était gelée, et la réparer n'a rien changé

Date : 2026-08-18

## Le défaut

`sub_821A5040` tient en trois instructions :

```c
r11 = [0x82000700];          // slot d'import KeTimeStampBundle, ordinal 173
return *(uint32_t *)(r11 + 16);
```

C'est le compteur milliseconde du guest. Le slot n'étant pas patché, il vaut
`(1 << 16) | 173 = 0x000100AD`, et la fonction déréférence `0x000100BD` —
adresse basse, non alignée — et **rend 0**. Elle est appelée **23 644 fois**
sur un run de 12 000 ticks, du tick 0 au tick 11 999.

## La réparation

Le bridge alloue un `KeTimeStampBundle` de 24 octets et écrit son adresse dans
le slot, en fin de `prepare()`. `set_tick` y entretient `TickCount` à
`tick * 1000 / 60` — le même profil 60 Hz que la base de temps 50 MHz de
`ppc.cpp`. `InterruptTime` et `SystemTime` restent nuls : **aucun lecteur de
ces deux champs n'existe dans cette image**, et les inventer n'aurait pas de
contrôle.

Le patch refuse d'agir si le slot ne contient pas exactement l'encodage non
patché, plutôt que d'écraser une valeur qu'il n'a pas posée.

Vérifié à l'exécution avant de conclure quoi que ce soit :

```text
AC6_KERNELDATA KeTimeStampBundle slot=0x82000700 was=0x000100AD now=0x0F000000
AC6_KERNELDATA tick=0   TickCount=0 ms
AC6_KERNELDATA tick=200 TickCount=3333 ms
AC6_KERNELDATA tick=400 TickCount=6666 ms
```

## Le résultat : rien

Run de 12 000 ticks avec START au tick 3000, atlas complet, comparé au même
run avant la réparation :

```text
fonctions atteintes   avant 2779   après 2779   nouvelles 0   perdues 0
sub_821A5040          23 644 appels dans les deux
anneau                submissions=2, wptr=25   inchangé
présentations         11 863                   inchangé
état du titre         1 au tick 2452, immobile jusqu'à 12 000
```

Pas une fonction de plus, pas une de moins. L'hypothèse — « le jeu attend un
délai qui ne s'écoule jamais » — est **fausse**, et c'est une réponse, pas un
échec : elle retire une explication qui restait plausible tant qu'on ne
l'avait pas essayée.

## Un piège du bridge, rencontré et documenté

Le premier essai allouait le bundle via `allocate_address()`. Ce compteur sert
aussi aux allocations du **guest**, donc consommer une page a déplacé le tas
invité, et avec lui le tampon d'écriture de scratch Xenos que le contrat épingle
à `0x16A5B000` / `0x16AE2000`. Résultat : `unqualified Xenos scratch writeback
target` au tick 0. La garde a fait exactement son travail.

Le bundle occupe désormais une page dédiée en `0x0F000000`, sous l'origine de
l'allocateur, mappée uniquement là. À retenir : **toute allocation nouvelle
faite avec `allocate_address` déplace la carte mémoire invitée** et casse les
contrats qui la fixent.

## Non établi

- Les neuf autres imports de données restent non patchés. `VdGlobalDevice` est
  le plus notable : `sub_821C64E8` y **écrit** l'objet device et
  `sub_821C5190` le relit, donc l'aller-retour passe par l'adresse bidon
  `0x000101BE` et fonctionne par accident. Le réparer est correct mais ne
  changera vraisemblablement rien non plus — à mesurer, pas à supposer.
- Ce qui, lui, tient le frontend.

# AC6 cycle 311 — la corruption de registres est localisée, et mes retraits sont hors de cause

Le cycle 310 avait établi que l'état des registres invités contredisait le code
traduit, et soupçonnait les 1 660 coupures `[functions]` retirées aux cycles 305
à 307. Ce cycle instrumente le site d'appel, localise la corruption à une
fenêtre de quelques instructions, et **réfute le soupçon**.

## 1. Instrumentation

Quatre accroches `midasm` posées dans `sub_821D4ED0`, avec identité du fil hôte
pour distinguer les quatre invocations concurrentes :

| Adresse | Rôle |
| --- | --- |
| `0x821D4ED0` | entrée de fonction, relève `r3` |
| `0x821D4F30` | avant le `bctrl`, relève `ctr`, `r28`, `r30` |
| `0x821D4F34` | site d'appel A, relève `r28`, `r30` |
| `0x821D4FB8` | site d'appel B, relève `r28`, `r30` |

L'invariant vérifié ne dépend pas de `r3` — ce qui est nécessaire, les deux
sites étant atteints via `mr r3,r28`. Il s'énonce simplement :
**`r28 == r30 + 16` doit tenir partout dans la fonction**, `r28` et `r30` étant
non volatils et écrits une seule fois chacun dans le prologue.

## 2. Mesure

```
CS entry tid=6975: r3=0
CS entry tid=bb05: r3=1
CS entry tid=8ea8: r3=2
CS entry tid=ad43: r3=3
CS probe [site B 0x821D4FB8] tid=bb05: r30=0x829e6540 r28=0x829e6550 invariant=held  index=1
CS callee              tid=bb05: target=0x821ccbe0 r30_in=0x00000000 r28_in=0x826a19b0
CS probe [site A 0x821D4F34] tid=bb05: r30=0x00000000 r28=0x826a19b0 invariant=BROKEN index=-1
```

Trois conclusions immédiates.

**Les quatre fils entrent correctement.** `r3` vaut 0, 1, 2, 3 ; le prologue
s'exécute. Aucune entrée « par le milieu » de la fonction.

**La corruption a lieu à l'intérieur d'une seule invocation.** Le fil `bb05`
tient l'invariant au site B — `r30 = 0x829E6540`, indice 1, exactement la valeur
attendue — puis le perd. Ce n'est donc pas une confusion entre fils.

**L'appel indirect n'est pas le coupable.** L'accroche posée *avant* le `bctrl`
montre `r30 = 0` et `r28 = 0x826A19B0` : les registres sont **déjà** corrompus
en amont de l'appel. L'hypothèse « le callee ne préserve pas les non volatils »
tombe pour cette cible.

La fenêtre de corruption est donc réduite au trajet entre le site B et le retour
en tête de boucle, soit `RtlLeaveCriticalSection`, `sub_821F41D0`,
`sub_821F5718`, `sub_821F40E8`.

## 3. Ce que la fenêtre contient

Aucun des trois appelés directs n'écrit `r28` ni `r30`. Au deuxième niveau,
`sub_821F74F8` écrit les deux — mais il est **bien formé** :
`__savegprlr_28` en tête, `__restgprlr_28` à l'unique sortie, un seul `return`.
Il n'explique pas la corruption.

En revanche `sub_821D4ED0` lui-même appelle `__savegprlr_27` et **n'appelle
jamais `__restgprlr_27`** : il sauvegarde `r27`–`r31` sans jamais les restaurer.

## 4. Réfutation : mes retraits de coupures ne sont pas en cause

Le cycle 310 posait que les 1 660 coupures retirées aux cycles 305–307, aucune
qualifiée par un contrat de frontière, pouvaient produire ce genre d'écart.
C'est vérifiable directement : le clone de référence conserve les frontières
d'origine.

Même analyse d'appariement `savegprlr`/`restgprlr` sur les deux corpus :

| Corpus | Fonctions | Sauvegarde sans restauration | Restauration sans sauvegarde |
| --- | ---: | ---: | ---: |
| référence, frontières d'origine | 21 432 | **42** | 5 110 (23,8 %) |
| après retrait de 1 660 coupures | 21 186 | **42** | 5 029 (23,7 %) |

Les chiffres sont **identiques** — 42 dans les deux cas — et le second est
marginalement meilleur en proportion. **Les retraits n'ont introduit aucun
déséquilibre.** Le soupçon du cycle 310 est levé.

Corollaire : 23,8 % des fonctions restaurent sans sauvegarder *dans le corpus
d'origine*. Ce n'est donc pas un défaut mais le motif normal — sauvegarde en
ligne (`std r28,-8(r1)`), restauration par l'assistant. Toute analyse future qui
traite « restauration sans sauvegarde » comme une anomalie part d'un faux
postulat.

`sub_821D4ED0` appartient en revanche aux **42** fonctions qui sauvegardent sans
restaurer, dans les deux corpus. C'est une population assez petite pour être
examinée exhaustivement.

## 5. État

- Corpus : **0 `REX_FATAL`**, 48 unités.
- Frontière de démarrage : 1,43 s, inchangée.
- Cause de l'abort : corruption de `r28`/`r30` dans une fenêtre de quelques
  appels, **localisée mais pas encore attribuée**.

## 6. Prochaine tranche

1. Encadrer la fenêtre par des accroches supplémentaires — après
   `RtlLeaveCriticalSection`, puis après chacun des trois appelés — pour
   réduire la corruption à un appel unique. C'est mécanique et décisif.
2. Examiner les **42** fonctions qui sauvegardent sans restaurer : population
   petite, présente dans le corpus d'origine, et `sub_821D4ED0` en fait partie.
3. Vérifier ce que les exports noyau `RtlLeaveCriticalSection` et
   `NtClearEvent`/`NtPulseEvent` font du contexte invité : ce sont des fonctions
   hôtes qui reçoivent le `PPCContext` complet et pourraient écrire des
   registres qu'elles devraient préserver.

`recompiler-generated` n'est pas `verified`.

# L'atlas nomme les deux slots, et invalide mon diff précédent

Date : 2026-08-18

## D'abord : l'instrument

Le commit `651e7878` a rejeté un diff de reachability fondé sur
`report.control_flow.edges`, parce que cette liste ne contient que les appels
**indirects** : `0x820CDF88` y apparaissait « manquante » alors que l'état 0 du
titre l'appelle en direct et aboutit. Le rejet était juste.

Le bon instrument existait déjà : `probe --atlas <fichier>` (qui exige
`--xam-movie-record`). Il produit la reachability **complète**, appels directs
compris — 2 711 fonctions sur la route neutre, 2 779 avec START, contre 222
extrémités indirectes dans la famille `swg` auparavant.

## La chaîne d'armement du rendu, mesurée pour de bon

Sur 12 000 ticks, les deux routes :

```text
ctor CX360UnitManager 0x82093840   neutre non   START non
ctor                  0x82095958   non          non
ctor                  0x82099F20   non          non
ctor                  0x82174888   non          non
ctor                  0x82176930   non          non
ctor                  0x8217C0E8   non          non
ctor                  0x8217C258   non          non
slot +0x14            0x820A45E0   non          non
lève (17,6)           0x820A4778   non          non
callback              0x821ADAB8   non          non
soumetteur            0x821C57D0   OUI          OUI
```

Le dernier maillon tourne ; tous ceux qui l'arment sont morts. C'est cohérent
avec `submissions=2` sur 12 000 ticks, et c'est maintenant un négatif valide,
pas une lecture d'un instrument incomplet.

## Par où les constructeurs devraient être atteints

Graphe d'appels directs inversé sur le C++ généré (7 956 callees) : aucun
appelant atteint à six niveaux au-dessus des sept constructeurs. Leurs cônes
sont très courts et se terminent sur trois fonctions sans appelant direct :

```text
0x8217C0E8, 0x8217C258  <- 0x8217C448 / 0x8217C490 <- 0x8217C4D8   (hors vtable)
0x82174888              <- 0x8217B668
0x82176930              <- 0x8217B6F8 <- 0x8217B788
```

et les deux dernières sont nommées :

```text
0x8217B668  slot +0x00 de CX360MissionManager<CAce6MissionManagerReplay>
0x8217B788  slot +0x00 de CX360MissionManager<CAce6MissionManagerCampaign>
```

Le gestionnaire d'unités n'existe donc que si un `CX360MissionManager<...>`
est employé. Rien dans la démo n'en emploie un sur ces 12 000 ticks.

## Les deux slots qui portent l'avance du frontend

La chaîne du tick 4251, celle qui fait avancer le titre dans le run neutre et
qui disparaît avec START, se nomme entièrement :

```text
0x820EA500 -> 0x8217C890   slot +0x54, partagé par CModeTaskTitle,
                           CModeTaskTitleDemoOffline, CModeTaskTutorialSelect,
                           CModeTaskGameDataSelect, …  (7 vtables)
0x8217C8B8 -> 0x8218AB98   slot +0x48 de CModeTaskTitleDemoOffline
0x8218A7F4 -> 0x8218AA30   bras d'état 2 de CModeTaskTitle::update
```

Le film ActionScript appelle donc un **dispatcher de commandes de script**
(slot +0x54 de la base des tâches de mode), qui appelle le **gestionnaire
propre au mode** (slot +0x48), qui fait avancer l'état. Avec START, le
dispatcher `0x820E9130` choisit d'autres cibles et le slot +0x54 n'est jamais
appelé.

## Non établi

- Ce qui appelle `0x8217C4D8` : ni appelant direct, ni entrée de vtable. Table
  de saut ou entrée intérieure ; non tranché.
- Si l'absence de `CX360MissionManager<...>` est normale hors mission, ou si
  c'est déjà le manque.
- Pourquoi le film cesse d'émettre la commande qui atteint le slot +0x54.

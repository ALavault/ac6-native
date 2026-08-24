# AC6 PAL démo — logo Namco naturel et mode manager qualifié, cycle 1786

Verdict : **RENDERER-GATE-CLOSED / FRONTEND-OPEN**, `supported=false`.

> Supersédé pour la frontière frontend par le cycle 1787. Le listener global
> `0x826DF804` et `E000004C/0x821A8C88` ont été réfutés comme arête manquante.
> Le vrai propriétaire est `CModeTaskTitleDemoOffline`, distinct de
> `CModeTaskMissionTitle`; toute la famille `CModeTask*DemoOffline` est
> démo-only et ne doit jamais être inférée depuis les prototypes retail.

Deux cold runs codegen-ON indépendants sélectionnent naturellement
`list 2 -> draw_index 2 -> handle 0x0E000059`, consomment un Q1 invité non
nul et guest-owned, font passer `921600/921600` samples et produisent le même
readback 1280x720 non uniforme avec un logo Namco centré reconnaissable. Les
traces sont égales octet par octet (11 577 727 octets, SHA-256
`389ba502569afa3c9b7956b84f98f5a777aa311a446dda7955cc5daed5bb889b`)
et les PPM également (SHA-256
`6c0ab7ac019e87bc17234c7a5b4189198a5d416717715916c24d3db25f559cac`).
Aucun draw, handle, Q1, état guest, scheduler ou pixel n'est forcé. Les
couleurs restent incorrectes, mais la chaîne producteur SWG -> draw ->
resolve -> writeback -> image visible est fermée.

La requalification read-only dans le projet Ghidra canonique
`ghidra-projects/ace-combat-6-demo` confirme ensuite que :

- `0x8218E970` est une fonction globale de sélection de factory ;
- `0x8218EA88` est le setter d'instance `CTaskModeManager+0x10` ;
- `0x82190B18` est une méthode d'instance qui construit, range le nouveau
  mode dans `+0x08`, l'insère via `[*0x82822F08]->slot+0x0C`, puis appelle
  son slot `+0x2C` ;
- `0x827435F8` est une cellule globale de type `CTaskModeManager*` ;
- le premier bord causal ouvert est le prédicat guest qui arme
  `CTaskModeManager+0x18`.

Cette preuve ne justifie aucun shim. `CModeTaskGameDemoOffline` est propre à
la démo PAL : son absence des prototypes retail Preview de septembre et
d'octobre est attendue et ne constitue pas une divergence.

Le readback correspond à l'époque précoce du logo Namco, pas au prompt
utilisateur beaucoup plus tardif `PRESS START`. Le gate suivant reste donc la
chaîne statique qui produit les états `+0x28/+0x2C`, arme `+0x18`, puis rejoint
le listener `0x826DF804`, le réveil `E000004C` et la reprise autour de
`0x821A8C88`. Une trace ne sera autorisée que si une arête causale nommée reste
indécidable après épuisement des preuves statiques.

Preuves autoritaires :

- [`title-vertex-window-runtime-20260823/RESULT.md`](../artifacts/goal-playable/title-vertex-window-runtime-20260823/RESULT.md) ;
- [`title-vertex-window-runtime-20260823-r2/RESULT.md`](../artifacts/goal-playable/title-vertex-window-runtime-20260823-r2/RESULT.md) ;
- [`frontend-mode-manager-canonical-ghidra-20260823/RESULT.md`](../artifacts/goal-playable/frontend-mode-manager-canonical-ghidra-20260823/RESULT.md).

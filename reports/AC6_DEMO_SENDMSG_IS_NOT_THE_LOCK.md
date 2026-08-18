# `SendMsgI` n'est pas le verrou, et deux erreurs d'arithmétique

Date : 2026-08-18

## La piste, et son refus

Après START, le film appelle `SendMsgI(S)->I`. Le rapport de trace montrait la
cible `0x820AC748` — le stub no-op partagé — ce qui ressemblait à un message
émis vers un gestionnaire absent, donc à un script bloqué en attente de
réponse.

`sub_820E9838` diffuse en fait sur un tableau d'auditeurs en `0x826DF800` et
appelle le slot `+0x20` de chacun. Dump à l'exécution :

```text
tick 222   [0]=0x18BA2C08 vptr=0x8200A584
tick 266   [0]=…          [1]=0x2E7F00E8 vptr=0x820112AC
tick 2429  [0]=…
tick 2452  [0]=…          [1]=0x2E3D0168 vptr=0x82011384
```

soit :

```text
0x8200A584  CSelectMessageDlgManager      auditeur permanent
0x820112AC  CModeTaskStartUpDemoOffline   vtable secondaire, objet mode+0x68
0x82011384  CModeTaskTitleDemoOffline     idem
```

Le mode courant s'inscrit donc bien comme auditeur, par sa seconde base. Et le
slot `+0x20` vaut `0x820AC748` — le no-op — **dans les trois vtables**. Aucune
n'en fournit d'implémentation.

Le message est donc diffusé à deux auditeurs qui, par conception, ne le
traitent pas. Ce n'est pas une requête restée sans réponse, et cette piste est
abandonnée plutôt que publiée.

## Une seconde piste refusée : `GetCurrentMode`

`GetCurrentMode()` rend 0 pendant tout le run, et aucun des 17 écrivains de
`[gs+120]` n'est atteint. Cela ressemblait à une lacune. Ce n'en est pas une :
le slot `+0x0C` de `CModeTaskTitleDemoOffline` — celui qui porte l'écriture
dans les classes qui en ont une — vaut `0x820AC748`. Le titre **n'enregistre
pas** de numéro de mode. 0 est la valeur correcte.

Cette vérification a coûté deux minutes et évité de publier une lacune
inexistante.

## Deux erreurs d'arithmétique, dont une trouvée par la mesure

`lis rN,imm` charge `imm << 16`, et j'ai converti l'immédiat signé de travers
deux fois :

- `-32195` lu comme `0x821D0000` au lieu de `0x823D0000` — corrigé avant
  publication en constatant que l'adresse pointait sur une instruction ;
- `-2106720256` lu comme `0x827E0000` au lieu de `0x826E0000` — la sonde n'a
  rien imprimé, ce qui l'a révélé.

La seconde n'a coûté qu'un run parce que la sonde était silencieuse plutôt que
fausse. La première aurait pu passer.

## Une ressemblance de chiffres qui ne veut rien dire

L'objet d'état de jeu est en `0x82774B00`. `CLAUDE.md` porte une question
ouverte sur `0x00074B00` contre `0x0007CB00`. Ce sont des **flags système de
shader ReXGlue**, sans aucun rapport. Noté ici pour qu'un lecteur pressé ne
refasse pas le rapprochement.

## Ce qui reste

La seule anomalie mesurée demeure `submissions = 2` sur 12 000 ticks, avec
toute la chaîne d'armement du rendu non atteinte. Le frontend, lui, répond à
l'appui. La priorité passe donc au constructeur absent de
`CX360MissionManager<...>` / `CX360UnitManager`, et non à la suite du script.

## Non établi

- Si le film, après START, a changé d'écran à l'intérieur de la même tâche de
  mode — ce qui rendrait l'absence de `menu_endMode` normale. Rien ne le
  tranche ici, et c'est la lecture qui rendrait le frontend sain.

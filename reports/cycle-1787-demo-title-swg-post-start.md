# AC6 PAL démo — vrai Title et prédicat SWG post-START, cycle 1787

Verdict : **TITLE-ROUTE-QUALIFIED / FRONTEND-OPEN**, `supported=false`.

> **SUPERSEDED par le cycle 1788.** La taxonomie du vrai Title et la chaîne
> conditionnelle après `menu_endMode` restent valides. En revanche, la
> « route naturelle de complétion » et la frontière
> `M102/lookup 0x0B -> EndMode` ci-dessous ne le sont pas : START retire le
> handler type 6 de la table de frame et conserve un chemin parallèle type 4.
> Voir `cycle-1788-demo-title-swg-frame-switch-correction.md`.

Le renderer reste fermé par les deux cold runs naturels identiques du cycle
1786. Ce cycle est exclusivement statique et corrige l'identité de la tâche
qui possède le vrai titre de la démo.

## Séparation de corpus et d'objets

La démo PAL qualifiée contient quatre types RTTI distincts :
`CModeTaskStartUpDemoOffline`, `CModeTaskTitleDemoOffline`,
`CModeTaskLoadingDemoOffline` et `CModeTaskGameDemoOffline`. Toute cette
famille est propre à la démo et absente des prototypes retail Preview de
septembre et d'octobre. Aucun constructeur, layout, slot, attribut ou nom de
méthode `*DemoOffline` n'est transférable depuis ces prototypes.

Dans la démo elle-même, `CModeTaskMissionTitle` est encore un autre type : sa
vtable primaire `0x8200E5C4`, son RTTI `0x823918C4` et son update
`0x82185198` le distinguent de `CModeTaskTitleDemoOffline`. Tout ancien join
qui faisait de `CModeTaskMissionTitle` le propriétaire du prompt `PRESS
START` est supersédé.

## Taxonomie du vrai Title démo

- `0x82192680` est une fonction globale de factory : elle alloue `0x78`
  octets puis appelle le constructeur d'instance `0x8218DC10`.
- `0x820113E4` est la vtable primaire globale constante de
  `CModeTaskTitleDemoOffline`; `0x82011384` est sa vtable secondaire
  `CSwgListener`, installée dans le sous-objet à `this+0x68`.
- `this+0x1C` est un sous-objet `CSwgManager`, distinct du listener.
- `0x8218A7A8`, `0x8218AB98`, `0x8218AA30` et `0x8217C890` sont des
  méthodes d'instance/overrides, pas des fonctions globales de transition.
- `Title+0x0C` est l'état, `Title+0x44` le compteur borné à trois updates et
  `Title+0x70` la valeur sélectionnée par le callback. `0x827435F8` est la
  cellule globale mutable qui contient `CTaskModeManager*`; `manager+0x18`
  est son champ d'armement.

La route aval conditionnelle, **si `menu_endMode` est invoquée**, est jointe :

```text
SWG menu_endMode 0x820EA4A8 / site 0x820EA500
  -> CSwgListener secondaire +0x68, slot +0x54
  -> 0x8217C890
  -> parent vtable slot +0x48 = 0x8218AB98
  -> Title+0x70, Title+0x44=3, Title+0x0C=2
  -> update 0x8218A7A8 pendant trois tours
  -> slot +0x4C = 0x8218AA30
  -> manager+0x18=1
  -> CTaskModeManager::update 0x821929A8
  -> transition 0x82190B18
```

Le listener global `0x826DF804` et la boucle événementielle
`E000004C / 0x821A8C88` sont des infrastructures séparées. Leur rôle comme
arête manquante vers `manager+0x18` est statiquement réfuté.

## Frontière post-START

Le SWG réel `brandLogo`, extrait de `DATA.TBL[170]/[171]`, contient la chaîne
et le statement `EndMode`. Après l'appui naturel au tick 3000, le burst du
tick 3001 appelle exactement `GetCurrentLevel`, `GetCurrentMission`,
`GetCurrentMode`, `SendMsgI("M102")` et `sound_onVoice2D`, puis le script du
titre devient silencieux. `M102` retourne un entier typé `0`; le statement
`EndMode` existe dans le buffer heap reconstruit mais n'est jamais fetché.

La frontière exacte est donc :

```text
résultat M102 ou lookup de catégorie 0x0B
  -> prédicat script qui sélectionne ou saute EndMode
  -> menu_endMode
```

Les deux causes restent compatibles; aucune n'est promue. `M150` appartient
au chemin loading et ne doit pas être joint au titre. La preuve statique
s'arrête au loader bitstream `0x82278F78`, dont la provenance complète du
payload SWG vers le buffer d'instructions heap reste ouverte.

Le prochain gate épuise ce lecteur bitstream et le branchement de
l'évaluateur. Si cette arête nommée reste indécidable, une seule trace sans
A/B est autorisée : START au tick 3000, release au tick 3001, entrée sur le
statement/lookup `0x0B` du burst, observables limitées au résultat lookup,
au boxed integer `M102`, au test de branche et au premier fetch éventuel de
`EndMode`; arrêt dès que le test qui sélectionne ou saute `EndMode` est
identifié.

Preuves autoritaires :

- `artifacts/goal-playable/title-start-route-static-20260823/RESULT.md` ;
- `artifacts/goal-playable/start-title-object-rtti-static-20260823/RESULT.md` ;
- `artifacts/goal-playable/title-swg-post-start-static-20260823/RESULT.md` ;
- `artifacts/goal-playable/listener-event-manager-static-audit-20260823/RESULT.md` ;
- `artifacts/goal-playable/prototype-september-proto2-transfer-audit-20260823/RESULT.md`.

Aucun shim, état guest forcé, appel de factory, modification du scheduler ou
changement produit n'a été effectué. `frontend=false`, `mission=false`,
`terminal=false`, `supported=false`.

# Cycle 1791 — HSIO contract et restauration Startup → Title

Date : 2026-08-23  
Cible : démo PAL `Default.xex`  
SHA-256 XEX : `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`  
Projet Ghidra canonique : `ghidra-projects/ace-combat-6-demo`

## Verdict

`GO_GUEST_STARTUP_TO_TITLE / FRONTEND_MILESTONE_OPEN`.

Le contrat HLE `VdIsHSIOTrainingSucceeded` renvoie maintenant le résultat
guest `0` lorsqu'aucun producteur d'état HSIO n'est implémenté. Le handler ne
modifie ni le global PAL `0x827AD310`, ni le device, ni le scheduler : le
guest prend lui-même sa branche de fallback. La revue statique qualifie ce
choix de service HLE fail-closed, sans établir que HSIO est l'unique cause
causale de la progression aval.

## Run final borné

Le script `artifacts/goal-playable/hsio-training-fallback-runtime-final-20260823/run.sh`
vérifie le binaire codegen-ON SHA-256
`541b7824c78f1ad70dbadbc67ffc0ca0e7ffa25d1d9b4678985fef6b984f9dd8`, le
manifeste codegen `465c279f529932165cd1d00416ff9b3d237ba3932a01874aea6cda6448994c76`
et les neuf fichiers du store. Le run est sans entrée, avec
`SDL_AUDIODRIVER=dummy`, Xvfb, backend `headless`, observateur mode borné par
la fenêtre `150:2801`, observateurs SWG activés, et run global borné par
`max_ticks=3200`.

Le résultat brut est `probe.status=4`, issue normale `max_ticks` et non un
trap. Les deux artefacts canoniques trace/report sont identiques au premier
run :

```text
probe.ac6rtply       31c72f30c00c2092943dcc52cdc1dca33bba068539f8c735c95d685b30aba9c7
probe.report.json    7e57e8b092664492511013dc49000e769a594145e7e77bf6abafa9da1aa5711c
```

Le report brut donne 3200 ticks, 3092 notifications `VdSwap`,
`frontend=false`, `mission=false`, `terminal=false`. Aucun present graphique
ou readback visible n'est promu par ce run.

## Chaîne guest fermée

```text
tick 2365  0x820EA4A8 / row 0x82386628 (EndMode)
            → listener Startup 0x821728C0
tick 2365  Startup+0x0C : 1 → 2
tick 2368  manager+0x18 : 0 → 1
tick 2368  0x82190B18 : manager+0x0C = Startup,
            manager+0x08 = nouvelle instance
tick 2369  current vtable = 0x820113E4 (CModeTaskTitleDemoOffline)
tick 2384  Title+0x0C : 0 → 1
tick 2385  Title reste courant, listener secondaire 0x82011384 visible
```

La cellule `0x827435F8` reste une globale contenant un
`CTaskModeManager*`; `manager+0x08/+0x0C/+0x18` sont des attributs de
l'instance manager. `0x82190B18` est une méthode d'instance, la factory
`0x82192680` est globale, et `0x8201130C/0x820113E4` sont des vtables globales
constantes. Aucun shim, remapping, état guest forcé ou pixel synthétique n'a
été employé.

## Frontière suivante

Le Title guest est maintenant établi avant toute qualification d'entrée, mais
le run final ne planifiait aucun START et ne ferme donc pas le frontend. Le
prochain travail reste statique d'abord : qualifier le producteur naturel du
prompt et la chaîne `pressed 0x82798488 → VM/SWG`, puis autoriser une seule
trace ciblée si la jonction vers `0x820EA4A8`/`manager+0x18` reste indécidable.
Le gate frontend exigera toujours un état guest persistant et une frame
visible post-transition. `supported=false` reste inchangé.

## Reçus associés

- `artifacts/goal-playable/hsio-training-fallback-static-review-20260823/RESULT.md`
  — SHA-256 `65a6afe02be56af1da99993e1c6187c401c2db8a18b3bfcf5a11eda14490f536`.
- `artifacts/goal-playable/hsio-training-contract-test-20260823/RESULT.md`
  — `build-final.status=0`, `ctest-final.status=0`, test ciblé `1/1`.
- `artifacts/goal-playable/title-prompt-start-next-static-20260823/RESULT.md`
  — frontière statique suivante, sans runtime ni modification produit.
- `artifacts/goal-playable/hsio-training-fallback-runtime-final-20260823/RESULT.md`
  — SHA-256 `122e234bfe9b46cef424e6f192c03c2e8c0cf8754b7b1a8446e95f93d90d2e54`.

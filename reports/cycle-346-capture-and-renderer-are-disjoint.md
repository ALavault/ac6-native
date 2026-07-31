# Cycle 346 — la capture et le renderer sont sur deux chemins disjoints

## 1. Question

`ac6_render_capture` est-il branché sur D3D alors que le rendu passe par Vulkan ?

## 2. Réponse : oui, et le câblage est lisible des deux côtés

**Côté capture.** `CaptureDrawCall` (`src/d3d_hooks.cpp:347`) est atteint depuis
trois sites, tous dans des **remplacements de fonctions invitées D3D** :

```
PPC_FUNC_IMPL(rex_sub_821DEF18)   // D3DDevice_DrawIndexedVertices  -> :502
PPC_FUNC_IMPL(rex_sub_...)        // variante indexée partagée      -> :530
PPC_FUNC_IMPL(rex_sub_...)        // primitive                      -> :705
```

Ce ne sont pas des `[[midasm_hook]]` mais des **surcharges** : le runtime
fournit le corps de la fonction invitée. Elles lisent le contexte PPC
(`ctx.r31`, `ctx.r25`, `ctx.r21`, `ctx.r22`).

Les adresses correspondantes sont bien déclarées et enregistrées :

```
0x821DEF18  config=1  présent dans generated/ac6recomp_init.cpp
0x821E2380  config=1  présent      (Clear)
0x821E10C8  config=1  présent      (SetTextureFetchConstant)
0x821E2BB8  config=1  présent      (Resolve)
0x821DBAF8  config=1  présent      (SetShaderGPRAlloc)
```

**Côté renderer.** Le compteur qui affiche 97 021 dessins hôte est
`backend_draw_telemetry`, produit par
`src/ac6_backend_fixes/ac6_backend_hooks.cpp`
(`ReportHostIssueCalled` / `ReportBackendResult` / `ReportHostDrawEmitted`),
lui-même appelé depuis :

```
thirdparty/rexglue-sdk/src/graphics/vulkan/command_processor.cpp:50
    #include ".../src/ac6_backend_fixes/ac6_backend_hooks.h"
```

**Le processeur de commandes Vulkan.** La surcouche le confirme :
`authoritative renderer: Vulkan`, `mode: hybrid_backend_fixes`.

## 3. Conséquence

Les deux chemins sont **disjoints**. La capture observe les appels D3D de
l'invité ; les dessins qui atteignent l'écran sont émis par le processeur de
commandes Vulkan à partir de l'anneau PM4. `capture draws 0/0/0` face à
`host draw 97 021` n'est donc pas une contradiction : **la capture est branchée
sur un chemin qui ne porte pas ce trafic.**

Cela ferme la question du cycle 345 et explique d'un coup :
`capture draws / clears / resolves: 0 / 0 / 0`,
`capture indexed / shared / primitive: 0 / 0 / 0`,
et `frame guest MATE: 0` — tous alimentés par le même chemin muet.

## 4. Réserve honnête

Une question reste ouverte et ne doit pas être présentée comme résolue :
l'invité **appelle** bien ses fonctions D3D, qui écrivent les paquets PM4. Que
les surcharges soient déclarées n'implique pas qu'elles s'exécutent. Deux
possibilités subsistent :

1. les surcharges se lient mais l'invité dessine par une autre entrée que les
   trois interceptées ;
2. les surcharges ne se lient pas à l'exécution.

Les départager coûte une ligne : `g_live_stats.draw_calls` est incrémenté
**sans condition** dans `rex_sub_821DEF18`, avant tout test du cvar. S'il reste
à zéro, la surcharge ne s'exécute pas.

## 5. Front suivant

L'attribution des 56 dessins doit se faire **côté Vulkan**, dans
`vulkan/command_processor.cpp`, où le trafic passe réellement : journaliser par
dessin la texture et le programme liés. `ac6_render_capture` n'est pas
l'instrument, quel que soit son état.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

# L'appui START entre dans une machine virtuelle de script

Date : 2026-08-18
Instrument : `AC6_DEMO_WATCH_THUNK`, sonde 3 200 ticks, appui au tick 3000

## Correction préalable

Le commit `e068b988` affirme que « rien ne dispatche par ce chemin
aujourd'hui, ce pourquoi une trace posée dessus a enregistré zéro
événement ». **C'est faux.** La trace a enregistré **31 événements** ; je l'ai
lue alors que le run n'avait pas encore atteint le tick 2991.

C'est la même erreur que celle commise sur `KeSetEvent` puis sur le sémaphore
`0xE0000130` : lire une mesure sur une fenêtre qui ne couvre pas l'événement,
et prendre le silence pour une absence. Trois fois dans la même session.

Ce qui reste vrai de ce commit : `0x820D32D0` et `0x820D3310` **sont** des
fonctions émises. Mais `AC6_PPC_CALL_INDIRECT` consulte le chemin trampoline
**avant** `lookup_guest_function`, donc l'émission ne le court-circuite pas.
Le chemin est vivant, pas résiduel.

## Ce que la trace montre

```text
tick 2991  thunk 0x820D3310  objet 0x2E412354  vtable 0x82006A9C  slot 0x64 -> 0x8219DC18
tick 3001  thunk 0x820D32D0  objet 0x2E413CD4  vtable 0x82006A9C  slot 0x70 -> 0x820D3AC8
tick 3001  thunk 0x820D32D0  objet 0x2E413994  vtable 0x82006A9C  slot 0x70 -> 0x820D3AC8
tick 3036+ thunk 0x820D3280  objet 0x2E41C054  vtable 0x82006B44  slot 0x6C -> 0x820D7F80
```

31 dispatches, sur deux vtables seulement : 26 sur `0x82006A9C`, 5 sur
`0x82006B44`.

## À qui appartient `0x82006A9C`

```text
tools/whose_vtable.py .build/Default.xex.base.bin 0x820D3AC8
    at 0x82006B0C   vtable 0x82006A9C slot +0x70
    .?AVInteger@?$ASContext@V?$lwallocator@E$0A@$0EA@@stx@@@swg@@
```

Démangé : `swg::ASContext<stx::lwallocator<unsigned char,0,64>>::Integer`.

Un `ASContext` — contexte ActionScript. **L'interface du frontend est pilotée
par une machine virtuelle de script**, et l'appui START y entre : au tick
3001, deux objets de ce type reçoivent leur slot `0x70`.

## Ce que cela déplace

Le consommateur de START n'est pas une méthode de `CModeTaskTitleDemoOffline`.
C'est un script. La décision de transiter appartient donc au script, et ce
qu'il attend pour la prendre est une question sur la VM et ses ressources —
pas sur la chaîne de modes.

Cela n'infirme pas la chaîne `CX360MissionManager` → `CX360UnitManager` →
armement du renderer, qui reste établie. Cela ouvre une seconde piste, en
amont : si le script ne dispose pas de ce qu'il attend, il ne demandera
jamais la transition qui construirait le gestionnaire de mission.

## Non établi

- Ce que fait `0x820D3AC8`, le slot `0x70` que l'appui invoque.
- Si la VM attend une ressource, un asset de script, ou un état.
- La part des appels virtuels **en ligne** dans le code généré, que ce
  trampoline ne voit pas : la trace ne couvre qu'une forme de dispatch.

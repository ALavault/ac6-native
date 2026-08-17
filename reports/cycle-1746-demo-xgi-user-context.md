# Cycle 1746 — import XGI neutral fermé

Cible exclusive : démo PAL `Default.xex`, SHA-256
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.
Aucune preuve retail n'est fusionnée.

## Preuve démo

Neutral atteint à tick 4254/thread 1 l'import `xam.xex:XMsgStartIORequest`,
ordinal 503, thunk `0x82375A44`, depuis le callsite PAL `0x821A559C` et
`LR=0x821A55A0`. Le tuple observé est :

| Registre | Valeur | Rôle ABI |
|---|---:|---|
| `r3` | `0xFB` | app |
| `r4` | `0x000B0006` | message |
| `r5` | `0` | overlapped |
| `r6` | `0x7F0409B8` | buffer |
| `r7` | `24` | longueur |

Une lecture GDB du binaire recompilé qualifié a capturé les six mots
big-endian du buffer :

```text
00000000 00000000 00000000 00000000 00008001 00000000
```

La table d'import PAL qualifie indépendamment l'ordinal 503 et son thunk. Le
checkout Xenia générique épinglé au commit
`95a5c3ee250f80c3b9d139658649d9ffb6db3eec` confirme seulement l'ABI cinq
registres : son `XgiApp` lit le message `0xB0006` de 24 octets et retourne le
succès sans écriture guest. Cette lecture est classée `xenia-generic`, jamais
preuve AC6.

## Implémentation fail-closed

Le runtime accepte uniquement le tuple démo exact : LR, app, message,
overlapped nul, longueur 24, plage mappée et six mots BE. Il ne modifie pas le
buffer ni un objet overlapped et retourne `r3=0`. Toute divergence rend la
dispatch non traitée et conserve le trap d'import existant.

Les tests couvrent le succès et les rejets pour LR, app, message, overlapped,
longueur, contenu et plage non mappée. Aucun handler générique des quatorze
autres callsites PAL n'est admis.

## Replay neutral

Deux exécutions neutral à 5000 ticks produisent des rapports et traces
byte-identiques :

- rapport : `6066da562acebd1d6596218c641676ea5cc3a36455f31b2f7c65d03e2cb2d6e2` ;
- trace : `f2c3f6a03095651767ea95617358f819fa99f726b7e1279bfdb173da652d286f` ;
- 4863 présentations ;
- sortie attendue `max_ticks`, 23 fibers guest bloquées à la fin ;
- `frontend=false`, `mission=false`, `terminal=false`.

Le binaire codegen-ON porte le SHA-256
`51f8d208b712db48c453d7f2c6de4d63e89967eb878e3443f0b67d8fd971dafc`.
CTest passe OFF 18/18 et ON 17/17, y compris complexité, audit source, Vulkan
et Python.

## Statut et prochain checkpoint

L'import ordinal 503 est fermé pour le seul appel atteint. L'atlas reste à
12 874 fonctions, couverture `.text` complète et SHA-256
`4e39111c83b9d124e02577fa707eb0815b2bfe2bc58ea4315f9691ae589230a2`.

Le handoff statique `ac6-demo-static-cross-analysis-main-handoff.md` est
intégré avec ses gardes : le shader `586168ec…a83cc0` est un VS pass-through
2D, mais l'indice `vf0/vf95` reste inconnu ; `0x821A3C30` ne reçoit pas le nom
PAC loader, `0x8227A5E0` reste sans rôle confirmé et les six VS PointSize
restent fail-closed.

Le prochain checkpoint est une extension neutral inchangée au-delà de 5000
ticks pour obtenir une nouvelle frontière causale. START reste gelé faute de
transition guest et visuelle ; aucun pixel non noir, audio ou résultat de
mission n'est promu.

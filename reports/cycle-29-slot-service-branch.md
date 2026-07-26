# Cycle 29 — branche post-slot AC6 PAL

Le Ghidra `-noanalysis` du XEX PAL qualifié confirme dans
`ghidra-cycle-29-slot-service-branch.log` la suite directe :

```text
0x82332318 -> 0x8233b790 -> 0x8233abe8
0x8233abe8 -> 0x823462a8
0x8233abe8 -> 0x823465f0
```

`Function_823462A8` effectue ensuite un appel virtuel sur son premier
paramètre et remet son champ `+0x8` à zéro ; `Function_823465F0` atteint une
branche marquée sans retour. Ces observations sont statiques et ne donnent ni
l'identité de service, ni capacité de slot, ni stabilité d'un hook. La
prochaine preuve reste une trace Xenia/XenonTests de consommateur réel.

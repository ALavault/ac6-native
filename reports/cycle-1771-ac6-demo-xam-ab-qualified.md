# AC6 PAL démo — A/B XAM qualifié, cycle 1771

Verdict : **XAM-AB-QUALIFIED / FRONTEND-NO-GO**, `supported=false`.

La capsule `1ed65d11…30bcce6c` a fermé deux probes froides sous l’identité PAL
démonstration, avec backend Vulkan, audio dummy et mapper route-aware :

| route | attente | chaîne | ticks | PRESENT | frontend | cible |
|---|---|---|---:|---:|---|---|
| `neutral` | `no_exclusive` | borne, 64 accès / 46 sites | 5600 | 5463 | false | lecture zéro |
| `buttons16` | `qualified_store_exclusive` | 58 accès / 40 sites | 5600 | 5463 | false | writer zéro |

Le writer est joint au projet Ghidra `ace-combat-6-demo`, à
`__imp__sub_822F5E58`, PC PAL `0x822F5EA0` (`90 7f 00 80`) et à l’adresse
runtime `0x829D153C + 0x80 = 0x829D15BC`. Les deux maps complètes sont
présentes dans la capsule content-addressed.

Ce checkpoint qualifie le transport et la différence d’accès guest, mais pas
l’effet sémantique : le writer stocke zéro, le readback reste noir et aucune
transition frontend/mission/terminal n’est observée. Les domaines MCP restent
`unavailable` et `supported=false`.

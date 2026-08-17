# AC6 PAL démo — premier lecteur guest du contrôleur, cycle 1772

Verdict : **CONTROLLER-CONSUMER-QUALIFIED / READBACK-NO-GO**, `supported=false`.

La capsule `1cdab9d8…e8cd6e8` rejoue deux probes froides jusqu’au tick 253
avec trace de lectures du bloc contrôleur `0x829D1550..0x829D15C3`. Après la
borne XAM, le premier lecteur guest hors chaîne de retour est identique dans
les deux routes :

| route | stop | première lecture | valeur | PC PAL | bytes |
|---|---|---|---|---|---|
| `neutral` | borne, 64 accès | `0x829D1580` (+0x44) | `0x00000000` | `0x822F61E4` | `81 7f 00 44` |
| `buttons16` | store exclusif qualifié, 58 accès | `0x829D1580` (+0x44) | `0x00000001` | `0x822F61E4` | `81 7f 00 44` |

Le site est `__imp__sub_822F61A0`, ligne générée 4456, thread 1, tick 252,
joint au projet Ghidra `ace-combat-6-demo` / `Default.xex`. La route
`buttons16` montre deux lectures de chaîne (`0x829D1558`, lignes 3976/4226)
avant ce premier consommateur non-chaîne ; elles ne sont pas promues comme
ownership joueur.

Le writer qualifié reste `0x829D15BC` (+0x80) et son effet/readback n’est pas
joint. Les deux routes s’arrêtent à 253 ticks (`PRESENT=116`), sans
frontend/mission/terminal. Aucun domaine MCP de gameplay n’est rendu
disponible et `supported=false` reste inchangé.

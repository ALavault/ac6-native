# AC6 PAL démo — jonction du mapper logique input, cycle 1775

Verdict : **INPUT-LOGICAL-MAPPER-QUALIFIED / MENU-CONSUMER-NO-GO**, `supported=false`.

La capsule `9de96a47…bc01` rejoue deux routes froides avec le binaire final
`cf90f551…e87b0`, jusqu'à `tick=253`. La trace jointe confirme la chaîne
guest `raw START 0x10 → normalized 0x400 → logical 0x10`, y compris le calcul
`pressed=current & ~previous`.

| route | normalized | logical current/previous/pressed | frontend | readback |
|---|---:|---:|---:|---|
| `neutral` | `0x00000000` | `0/0/0` | non | noir |
| `buttons16` | `0x00000400` | `0x10/0/0x10` | non | noir |

Les sites PAL/Ghidra `ace-combat-6-demo` des fonctions `0x821DE990`,
`0x821DE6E0`, `0x821DE778` et `0x82198BF0` sont rejoints aux instructions
PPC et aux accès dynamiques exacts. Les consommateurs statiques START
`0x82170FCC` et `0x82185210` ne sont toutefois pas observés avant la borne.

Les deux routes retournent `rc=4`, `PRESENT=116`, `frontend=false`,
`mission=false`, `terminal=false`, avec un readback noir. La livraison et le
mapper logique sont promus ; START, frontend, mission, gameplay et terminal
restent fermés, et `supported=false` ne change pas.

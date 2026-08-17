# AC6 PAL démo — snapshot input guest, cycle 1774

Verdict : **INPUT-SNAPSHOT-QUALIFIED / START-READBACK-NO-GO**, `supported=false`.

La capsule finale `35c1b11b…46aea` utilise le binaire `a93db7af…299dc` et
deux probes froides PAL à `tick=252`. Le consommateur XAM qualifié reçoit un
état XInput de 16 octets sur `controller+0x44`, puis le guest expose le même
objet `0x829D153C` et son vtable `0x8202A8DC`.

| route | current/pressed | normalized | logical current/pressed | readback |
|---|---:|---:|---:|---|
| `neutral` | `0/0` | `0x000` | `0/0` | noir |
| `buttons16` | `0x10/0x10` | `0x400` | `0x10/0x10` | noir |

La route `buttons16` est donc une observation guest non nulle, jointe aux
sites statiques de normalisation `0x82198C44` et de mapping logique
`0x821DE990`. Elle ne suffit pas à nommer ou promouvoir une transition START :
les deux routes terminent à `rc=4`, `253 ticks`, `PRESENT=116`,
`frontend=false`, `mission=false`, `terminal=false`, avec la même frontière et
le même readback noir. Les domaines de jeu MCP restent indisponibles.

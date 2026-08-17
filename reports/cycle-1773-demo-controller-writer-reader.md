# AC6 PAL démo — jonction writer→reader du contrôleur, cycle 1773

Verdict : **CONTROLLER-WRITER-READER-QUALIFIED / READBACK-NO-GO**, `supported=false`.

La capsule content-addressed `f975d130…d0177` rejoue deux routes froides
jusqu’à 5600 ticks. Le store qualifié de `buttons16` et le premier `lwz` guest
du même slot sont maintenant joints :

| élément | PC PAL | bytes | tick | valeur |
|---|---:|---|---:|---:|
| writer `stw r3,128(r31)` | `0x822F5EA0` | `90 7f 00 80` | 252 | `0x00000000` |
| reader `lwz r11,128(r31)` | `0x822F5EA8` | `81 7f 00 80` | 254 | `0x00000000` |

Les deux sites appartiennent à `__imp__sub_822F5E58`, sont mappés par le
mapper PAL/Ghidra `ace-combat-6-demo` / `Default.xex`, et ciblent
`0x829D153C + 0x80 = 0x829D15BC`. Le reader persiste aux ticks 255 et 763/765.
La route `neutral` lit également ce slot à tick 252, sans store exclusif ; la
route `buttons16` s’arrête sur `qualified_store_exclusive` (58 accès).

La jonction ferme seulement la frontière mémoire guest. La valeur écrite/lue
reste nulle, aucun effet readback non-noir n’est prouvé, et les deux routes
finissent à `PRESENT=5463`, sans frontend, mission ou terminal. Aucun domaine
MCP de gameplay n’est promu ; `supported=false` reste inchangé.

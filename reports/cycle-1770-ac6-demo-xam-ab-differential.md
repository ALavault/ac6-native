# AC6 PAL démo — A/B XAM différentiel, cycle 1770

Verdict : **XAM-AB-DIFFERENTIAL / FRONTEND-NO-GO**, `supported=false`.

Le job `job-mswzopyp-b73383bf` a exécuté deux probes froides avec l’identité
PAL démo, le binaire codegen-on et Vulkan. Les deux routes atteignent 5 600
ticks et 5 463 `PRESENT`, sans frontend, mission ou terminal.

| route | champ `0x829D15BC` | chaîne | résultat du mapper |
|---|---|---|---|
| `neutral` | lecture `0x00000000` | borne à 64 accès | refus attendu, aucun writer |
| `buttons16` | `store32 0x00000000` | `qualified_store_exclusive`, 58 accès | 58 lignes / 40 sites mappés |

La fermeture statique est jointe au projet Ghidra `ace-combat-6-demo`, à
`__imp__sub_822F5E58`, PC PAL `0x822F5EA0`, octets `90 7f 00 80`, et à l’objet
contrôleur qualifié `0x829D153C + 0x80 = 0x829D15BC`. Le writer est donc
qualifié comme accès guest, mais son effet reste non prouvé : il écrit zéro et
aucun consommateur/readback positif n’est joint.

La capsule complète reste content-addressed sous le reçu
`7ad7ea95…8033b44b`; les traces ne sont pas copiées dans Git. Le mapper a été
resserré sur les 40 PC et octets exacts du corridor, tout en refusant la route
neutre. Aucun claim frontend, mission, terminal, observation MCP ou parité
native n’est promu.

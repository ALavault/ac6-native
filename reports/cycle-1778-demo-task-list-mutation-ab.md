# AC6 PAL démo — mutation de task-list, cycle 1778

Verdict : **TASK-LIST-MUTATION-QUALIFIED / MENU-OWNER-NO-GO**, `supported=false`.

La capsule [task-list-ab](../analysis/demo/ac6-demo-task-list-ab/sha256/d37e15f4ba764ab257372468228a451262509dcfa0af4283f7321c90ad857628/receipt.json) scelle deux stores frais au binaire `e95a0947…1925`, avec `AC6_DEMO_WATCH_TASK_LIST=1`, jusqu'au tick 300.

| route | accès task-list | stores | loads | construction | résultat |
|---|---:|---:|---:|---|---|
| `neutral` | 1688 | 43 | 1645 | ticks 106, 221 | `rc=4`, 163 PRESENT, readback noir |
| `buttons16` | 1688 | 43 | 1645 | ticks 106, 221 | `rc=4`, 163 PRESENT, readback noir |

Les accès aux 16 mots `0x18970400..0x18970440` sont byte-identiques entre les
routes. Le remplissage initial `0xFEFEFEFE` survient au tick 4. Les écritures
qualifiées des ticks 106 et 221 construisent trois objets déjà connus :
`CTaskLoading` (`0x18BA2BF4`), `CTaskModeManager` (`0x18980000`) et
`CModeTaskStartUpDemoOffline` (`0x2E7F0080`). Les sites producteurs sont joints
aux bytes PAL dans le reçu, sans copier de C++ généré dans le produit.

Le dispatcher `0x82259D74`, slot 4, appelle ces trois vtables de façon
identique des ticks 252 à 299 (`48` appels par entrée). Aucun propriétaire menu
`0x82170F58`/`0x82185198` n'est construit ou observé. `frontend`, `mission` et
`terminal` restent faux ; la PRESENT seule ne ferme donc aucune gate.

La frontière suivante est une construction guest d'un propriétaire menu ou un
changement d'état menu persistant joint à un readback non noir. Aucune
observation MCP, mission ou terminal n'est promue.

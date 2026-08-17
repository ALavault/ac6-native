# AC6 PAL démo — A/B XAM return-chain négatif, cycle 1769

Verdict : **XAM-AB-NEGATIVE / FRONTEND-NO-GO**, `supported=false`.

Le job `job-mswwrng9-946059c2` a exécuté deux routes fraîches (`neutral` et
`buttons16`) avec l’identité PAL démo, le binaire codegen-on et le backend
Vulkan. Le job termine en échec contrôlé parce que le mapper refuse la trace
neutral ; il n’y a ni fichier `current` écrit ni claim frontend produit.

Les deux rapports de route sont néanmoins valides comme frontière négative :

| route | ticks | PRESENT | frontend | mission | terminal | frontier | readback normal |
|---|---:|---:|---|---|---|---|---|
| neutral | 5600 | 5463 | false | false | false | `0x822F8848` | `0b150fd3…ec58366` |
| buttons16 | 5600 | 5463 | false | false | false | `0x822F8848` | `0b150fd3…ec58366` |

Les traces et rapports restent dans la capsule content-addressed locale ; ce
checkpoint ne copie aucun gros conteneur. Les hashes des rapports, traces, du
binaire et du manifeste codegen sont scellés dans le JSON. La convergence de
la frontière et du readback noir ne qualifie pas le rôle sémantique du guest,
le START, le frontend ou la mission.

La prochaine fermeture doit corriger ou requalifier le mapper XAM avec un
replay A/B strict avant toute observation guest-owned. `supported=false` et
les domaines MCP `unavailable` sont maintenus.

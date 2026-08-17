# Cycle 1670 — absence de consumer guest frontbuffer jusqu’à 600 ticks

## Verdict

Le hook PPC de lecture frontbuffer est actif uniquement sous
`AC6_DEMO_WATCH_FRONTBUFFER_READERS=1`. Il observe les helpers générés
`AC6_PPC_LOAD_U8/U16/U32/U64`, avec adresse, largeur, valeur, PC/LR de contexte,
thread, tick, nom de fonction et ligne générée. Il ne modifie aucune valeur.

Neutral et START ont chacun écrit le resolve exact dans `GuestMemory`, mais
aucun load guest de `[0x1374A000,0x13AE2000)` n’apparaît jusqu’au tick 600.
Cette absence est une preuve négative bornée ; elle ne permet pas d’inventer un
consumer, une présentation native ou une screencap.

## Cible et instrumentation

- `Default.xex`, SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` ;
- Xenon big-endian / Xenos, démo PAL uniquement ;
- hook : `src/guest_bridge/frontbuffer_writer_trace.hpp`, chaîne appelée depuis
  les quatre `AC6_PPC_LOAD_*` de `src/guest_bridge.cpp` ;
- l’activation du hook de site de load reste opt-in ; les runs ordinaires ne
  changent pas de trace ;
- plage exacte : `0x1374A000..0x13AE2000`, soit `0x398000` octets.

## Résultats A/B

| route | ticks | RTPLY SHA-256 | lignes `AC6_FRONTBUFFER_GUEST_READ` | guest writeback | digest guest linéaire |
|---|---:|---|---:|---:|---|
| neutral | 253 | `c5357c6d9639c2a675161bd10c3cdbe97096df0dac11d760fe8a2d964b1c5794` | 0 | 1 | `0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f` |
| START tick 252 | 253 | `4a7326d9b1148dc6a5943cabea5c0e2562ae7ad833eda6bc7cd92a089e25724f` | 0 | 1 | `0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f` |
| neutral | 600 | `6c34827cc3f9962a7f4042610d69aeb54bbd0165fd5a1c830d341efad07970c7` | 0 | 1 | `0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f` |
| START tick 252 | 600 | `2a4577f883bbfa31f8740b35e998da10ea42ddba050fa7232e80abe6c71f27cc` | 0 | 1 | `0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f` |

Les quatre sorties ont stderr vide (`e3b0c442…2b855`), donc aucune ligne de
lecture n’a été filtrée par une erreur runtime. Les frames restent noirs et le
run ne franchit pas de milestone frontend/mission.

Le binaire codegen final utilisé vaut
`156d46feece454e9fd272bcf80e2d96622bb10bfec4d59251c8c5e1081f8698b`.
`tools/ppc_context_adapter.h` publie désormais aussi les sites U8/U16/U64;
son hash est `0707a79bf2a6749821c3085f4634433c0108e16c95239af95a9bb86d31e51214`.

## Classification

- `demo-qualified` : absence de lecture PPC dans cette plage, sous les bornes
  253/600 ticks, sur neutral et START ; writeback renderer relu identiquement ;
- `demo-observed` : deux routes atteignent le même resolve noir ;
- `xenia-generic` : aucun nouveau fait ;
- `unknown` : consumer différé, scanout/XE_SWAP host, persistance guest après
  600 ticks, pixels non noirs, frontend, mission et screencap.

Le caller du writeback cycle1669 est le renderer host `GuestMemory::store_bytes`,
pas un consumer guest. Aucune preuve retail, mutation Ghidra/Xenia/ReXGlue,
microcode ou actif propriétaire n’a été ajoutée.

## Prochain test ciblé

Étendre le hook à la frontière guest de `VdSwap`/interrupt et aux éventuelles
lectures vectorielles générées (si elles apparaissent dans l’atlas), sans
intercepter les reads host de `GuestMemory::load_bytes`. Si la plage reste
vierge après une route frontend qualifiée, conserver le renderer comme
writeback guest-owned mais ne pas promouvoir de screencap ; qualifier d’abord
le consumer/scanout exact ou rester fail-closed.

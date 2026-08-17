# Cycle 1668 — jointure `XE_SWAP` → readback Vulkan host

## Verdict

Le chemin Vulkan ne résout désormais le frame atteint qu’après avoir trouvé,
dans le même batch que le copy draw, un `XenosPresentCommand` dont l’adresse et
les paramètres égalent exactement le `XE_SWAP` PAL. Le draw normal peut avoir
été capturé dans le batch précédent, mais doit déjà être qualifié. Le buffer tiled host est borné,
comparé à l’oracle CPU puis untilé. La route est qualifiée côté host, mais
aucun writeback `VkBuffer` → `GuestMemory` n’est introduit : la preuve
guest-owned et la screencap restent ouvertes.

## Contrat démo vérifié

- cible exclusive : `Default.xex`, SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` ;
- `XE_SWAP`/fetch : adresse `0x1374A000`, format 6, tiled, 1280×720 ;
- destination tiled : `0x398000` octets, fin exclusive `0x13AE2000` ;
- le nouveau champ `XenosPresentCommand::physical_address` provient du
  packet atteint, sans nom retail ni allocation host substituée ;
- `qualify()` refuse toute variation d’adresse, format, tiling ou dimensions,
  et refuse l’absence ou la multiplicité de présentation dans le batch.

## Readback host

Après le dispatch ReXGlue fast32, les canaris avant/après restent intacts, le
buffer tiled complet égale l’oracle CPU, et le buffer untilé donne :

- hash tiled : `94831d4c398252020f792d92f546c5122ad522c4270b73be9e8619fde1db641f` ;
- hash linéaire RGBA8 :
  `0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f` ;
- `present_joined=true`, adresse `0x1374A000`, taille `3768320` octets.

Le contenu est noir pour ce checkpoint neutral/START. Le raw buffer reste
éphémère dans `TMPDIR`/mémoire mappée et n’est pas suivi dans le paquet.

## A/B runtime frais

Codegen ON, backend Vulkan, store démo PAL neuf, 253 ticks :

| route | résultat | RTPLY SHA-256 | readback normal | readback resolve |
|---|---|---|---|---|
| neutral | `play`, rc 0 | `c5357c6d9639c2a675161bd10c3cdbe97096df0dac11d760fe8a2d964b1c5794` | `0b150fd3…ec58366` | `0c660f2b…a4913a5f` |
| START tick 252 | `probe`, `max_ticks`, rc 4 attendu | `4a7326d9b1148dc6a5943cabea5c0e2562ae7ad833eda6bc7cd92a089e25724f` | `0b150fd3…ec58366` | `0c660f2b…a4913a5f` |

Le rapport START a le SHA-256
`2d0c391ba3d0d2a6f2dbf0907ac8538abde5be17f8f9424c2fb63cb2bb91a6cf`.
Les deux routes traversent le join strict ; aucune transition frontend,
mission ou terminal n’est promue.

## Validation et classification

- CTest codegen ON : 17/17 ;
- CTest démo OFF : 18/18 ;
- complexité et audit source : pass ;
- binaire codegen ON :
  `c4ecb2de22338e17ab251c7f7b37d88811c173b861dba50fd2e94605f4635dbb` ;
- `demo-qualified` : join packet→destination, bornes, canaris, hash tiled et
  hash linéaire ;
- `unknown` : writeback guest, contenu EDRAM guest-owned, premier consumer
  guest et pixels non noirs ;
- aucune preuve retail ou actif propriétaire fusionné/suivi.

Prochain checkpoint : instrumenter un transfert GPU→guest réel (ou démontrer
que le guest lit ce buffer via une adresse qualifiée) et joindre son writer,
son consumer et son hash avant toute screencap.

# Cycle 1671 — persistance VdSwap et absence étendue de consumer frontbuffer

## Verdict

Une exécution neutral Vulkan fraîche de processus atteint 800 ticks avec le
hook opt-in `AC6_DEMO_WATCH_FRONTBUFFER_READERS=1`. Le resolve exact est
réécrit dans l’allocation guest `0x1374A000`; le digest linéaire reste
`0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f`.
La plage `[0x1374A000,0x13AE2000)` ne reçoit aucune lecture par les quatre
helpers scalaires PPC observés. Le guest appelle toutefois `VdSwap` 663 fois,
la dernière notification étant au tick 799. Cela qualifie la persistance de
la notification de présentation, pas une lecture de pixels ni un scanout
guest-owned.

Le résultat reste fail-closed : aucune screencap n'est promue et le frontend,
la mission et le consumer vectoriel restent inconnus.

## Cible et méthode

- `Default.xex`, démo PAL Xbox LIVE, SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` ;
- Xenon big-endian / Xenos ; aucune preuve retail fusionnée ;
- binaire codegen utilisé :
  `156d46feece454e9fd272bcf80e2d96622bb10bfec4d59251c8c5e1081f8698b` ;
- hook de lecture uniquement dans `AC6_PPC_LOAD_U8/U16/U32/U64`, sans
  modification de valeur ; les loads vectoriels directs générés ne sont pas
  prétendus couverts ;
- backend Vulkan, `SDL_AUDIODRIVER=dummy`, `xvfb-run`, store démo qualifié
  existant, processus neuf ;
- commande : `probe --until frontend --max-ticks 800 --backend vulkan`.

## Artefacts et résultats

| élément | valeur |
|---|---|
| RTPLY neutral 800 | `bbc7ecb8fc20bfdbbdd1d70d24a4fdc78f6619dd5622c761e88541d776fd7068` |
| rapport frontier neutral 800 | `0d70fd74c24bd4205491c66c56bd4a5c31278dd99d6270a44248196990563ffa` |
| stdout renderer | `69f2acced5eeac19abd2b2bc8e9bb95e9ceabc5b4b69732d94e18c978418ec40` |
| stderr hook | vide, SHA `e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855` |
| ticks complétés | 800 |
| notifications `VdSwap` | 663, dernière au tick 799 |
| lignes `AC6_FRONTBUFFER_READ` | 0 |
| guest writeback | 1 |
| digest guest linéaire | `0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f` |
| présentation Xenos typée | 1, format 6, tiled, 1280×720 |

Le dernier snapshot `VdSwap` conserve `frontbuffer_address=0x1374A000`,
`texture_format=6`, `color_space=0`, 1280×720 et les six fetch words déjà
qualifiés. Le flux conserve les IB `0x127CA0C0/11` (`ef7ab6e4…d2b0`) et
`0x1274A000/3029` (`d121c8d8…358d6`).

Les A/B neutral/START à 253 et 600 ticks du cycle 1670 restent la preuve
comparative : zéro lecture scalaire dans la même plage pour les deux routes,
writeback=1 et digest guest linéaire identique. Une tentative START étendue
distincte s'est arrêtée avant sa borne sans rapport complet ; elle n'est pas
utilisée comme preuve.

## Qualification

- `demo-qualified` : adresse, dimensions, format/tile, writeback pixel-only,
  répétition `VdSwap` jusqu'au tick 799 et absence bornée de loads scalaires ;
- `demo-observed` : 663 notifications guest `VdSwap` et un PRESENT Xenos typé ;
- `xenia-generic` : aucun nouvel élément ;
- `unknown` : loads VMX/vectoriels directs, consumer guest, lecture de scanout,
  pixels non noirs, frontend, mission et screencap.

## Garde et prochain checkpoint

Ne pas transformer la notification `VdSwap` en consommation de pixels. Le
prochain test ciblé doit joindre une lecture vectorielle exacte (ou un signal
de scanout/interrupt qui lit réellement l'allocation) à un PC/LR/thread/tick,
sans modifier le C++ généré. Tant que cette jointure manque, le writeback
reste un effet renderer guest-owned qualifié mais aucune image jouable n'est
livrée.


# Cycle 1679 — adaptateurs atomiques et A/B Vulkan PAL

## Résultat

Les instructions `ldarx`/`stdcx.` du codegen strict sont maintenant routées
par les adaptateurs diagnostiques du runtime. Aucun déréférencement brut
`base + ...` ne subsiste dans les 52 sources générées fraîches. Le build et
les tests passent, puis deux stores neufs neutral/START sont rejoués à 253
ticks en headless et avec Vulkan.

La route Vulkan atteint le pipeline déjà qualifié : 5 chargements de shaders,
26 draws, 1 present, 1 draw normal, 1 resolve neutre, 4 modules Vulkan,
2 layouts de descripteurs et 2 pipelines graphiques. Les deux routes ont les
mêmes IB, microcodes, resolve, `XE_SWAP` et readbacks. Le writeback
`AC6_FRONTBUFFER_HOST_WRITE` est observé côté hôte, pas comme une lecture ou
écriture guest-owned.

## Identité et génération

| élément | valeur |
|---|---|
| cible | `Default.xex`, PAL démo, SHA-256 `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| XenonRecomp | commit `ddd128bcca99fe8bfbb99bea583c972351fa6ace` |
| patch strict | `recompilation/ace-combat-6-demo/patches/xenonrecomp-strict-recompiler.patch`, SHA-256 `c74303af6bcde78a3d9b10a96ee83f2c6f29517cb10c0e74a09e01969314ee40` ; `git apply --check` réussi sur clone temporaire |
| adaptateur | `tools/ppc_context_adapter.h` SHA `0f31a29b73a8a9ee5a35355dc33cfaa938a5cf561ce61e57a16524b519a06a4f` |
| manifest codegen | `.build/ac6-demo-codegen-atomic-1/manifest.json`, SHA `ae57c86869c556f17c7eee3f9d781d19943d20bf2599abbe201ac88954bda185` |
| sources | 52 fichiers, 55 662 296 octets, agrégat nom+bytes SHA `1a83381595fa7e5a502e3925f8bd3ac295552e6d2293d028a600ebcbdf64b210` |
| diagnostics | 0 frontière, 0 instruction non supportée, 52/52 générés/compilés |
| cache | `/usr/bin/ccache`, version 4.12.3, activé via `CMAKE_CXX_COMPILER_LAUNCHER=ccache` |

Le patch fait passer les charges atomiques par `AC6_PPC_LDARX` et les stores
conditionnels par `AC6_PPC_STDCX`. L'adaptateur exige l'alignement 8 octets,
conserve les deux générations d'écriture des mots 32 bits, invalide la
réservation après tentative et réutilise les helpers guest-endian existants.
Le comptage exact des sources générées, avant et après le patch, est 6
`ldarx` et 8 `stdcx.` (les commentaires 7/7 du cycle 1677 étaient une erreur
de comptage, corrigée ici). Les huit `stdcx.` comprennent les deux branches
de restauration des fonctions CAS; il n'y a donc pas de perte d'instruction.
La comparaison octet par octet confirme que seul le routage change, et aucune
expression mémoire brute ne subsiste dans la génération fraîche.

## A/B headless, 253 ticks

Stores neufs : `.build/ac6-demo-atomic-neutral-store` et
`.build/ac6-demo-atomic-start-store`, START `0x10` au tick 252.

| invariant | neutral | START |
|---|---|---|
| RTPLY SHA | `1d41d2e26003a631f8bec19534812a258ffc5d90466be885f6f7f3df797ebef7` | `31553733582cf7345375d8f1978a28645621e488700f2a31b7e883b66109360b2` |
| rapport SHA | `420fb9341ea0a56be33f0143f9bf9be768b08a1697fe7cb314e5e055bbf2a843` | `8506399dca66624afcf52a8746bc4cd08c669f30021d9fb954bb8bebb01f7327` |
| IB intermédiaire | `ef7ab6e4…d2b0` | identique |
| IB principal | `d121c8d8…358d6` | identique |
| stderr | SHA `e3b0c442…` (vide) | identique |
| résultat | `max_ticks`, 253 | `max_ticks`, 253 |

Les hooks frontbuffer guest readers/writers n'émettent aucune ligne. Les
milestones restent `frontend=false`, `mission=false`, `terminal=false`.

## A/B Vulkan, 253 ticks

Traces fraîches sous `/fastdata/lavaulta/tmp/ac6-demo-atomic-vulkan-ab.hPdUYa`.

| élément | neutral | START |
|---|---|---|
| rapport SHA | `4c6bea202bb0c2c86ecf102735ab4523c2f12411d6f241e8aa1ade1e8d77a214` | `d69ab8c7cf0e2d887f251ed62a5e803f1f51f5a9136e27aec2547dcfc2c88a35` |
| trace SHA | `c5357c6d9639c2a675161bd10c3cdbe97096df0dac11d760fe8a2d964b1c5794` | `4a7326d9b1148dc6a5943cabea5c0e2562ae7ad833eda6bc7cd92a089e25724f` |
| pipeline | 4 modules, 2 layouts, 2 pipelines | identique |
| renderer | 5 loads / 26 draws / 1 present / 1 normal draw / 1 resolve | identique |
| readback normal | `0b150fd32588b1daca5569992ebe559c0102c837306b1af4c44d35128ec58366` | identique |
| resolve linéaire | `0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f` | identique |

Les deux runs observent exactement un writeback hôte à
`0x1374A000`, taille `0x398000` (`3768320` octets), caller offset `0xBE447`.
Il s'agit de la frontière de présentation native déjà connue; aucune preuve
de consumer guest-owned, de pixels non noirs ou de screencap n'est promue.

Le ring reste `base=0x126CA000`, capacité 131072 dwords, deux soumissions,
25 dwords; IBs `0x127CA0C0/11` et `0x1274A000/3029` conservent leurs hashes
qualifiés. Le corpus PM4 est fermé (`877` packets, `3065` dwords décodés).

## Validation et classification

- CTest codegen/runtime : **17/17** sous `SDL_AUDIODRIVER=dummy` et Xvfb.
- `PPC_LDARX`/`PPC_STDCX` : génération compilable, transactionnelle et
  fail-closed; les adresses non alignées ou réservations périmées échouent.
- `demo-qualified` : identité XEX, codegen reproductible, absence de
  déréférencement brut, A/B IB/PM4/Vulkan/readback stables.
- `demo-observed` : writeback hôte du frontbuffer pendant les deux probes.
- `xenia-generic` : aucun nouvel élément.
- `unknown` : correspondance des microcodes aux containers, consumer guest
  de pixels, contenu non noir, frontend, mission et résultat.

## Prochain checkpoint

Le comptage statique est maintenant réconcilié à 6/8 avant et après le patch.
Instrumenter une nouvelle trace `rr` sur ces wrappers pour joindre PC/LR,
thread, tick et adresse effective. Toute adresse recouvrant
`[0x1374A000,0x13AE2000)` doit trapper avant effet. La route reste sans
screencap tant qu'un consumer guest-owned et un contenu pixel non noir ne
sont pas prouvés.

Politique : aucun retail fusionné, aucun checkout Xenia/ReXGlue/Ghidra
modifié, aucun C++ généré ou microcode suivi, aucun actif propriétaire ajouté.

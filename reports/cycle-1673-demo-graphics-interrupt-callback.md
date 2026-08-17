# Cycle 1673 — callback d’interruption graphique guest

## Verdict

La frontière `VdSwap → VdSetGraphicsInterruptCallback → callback` est
maintenant jointe dynamiquement sur la démo PAL. Neutral et START, chacun
depuis un processus neuf jusqu’au tick 800, enregistrent le même callback
guest `0x821B9710` avec le contexte `0x10041A00`, puis exécutent 800 appels:
un appel `source=1` au tick 1 et 799 appels `source=0` aux ticks 1–799.

Les hooks scalaires et vectoriels ne voient aucune lecture de
`[0x1374A000,0x13AE2000)`. Le callback est donc une frontière guest
réellement active, mais aucun consumer pixel/scanout n’est démontré. Le
résultat reste fail-closed et aucune screencap n’est promue.

## Preuve statique PAL

L’atlas démo qualifie `0x821B9710` par `.pdata`, bytes SHA-256
`b2d449272e73fed7f844bb29153b181922a4eb30fc84c2c09740503eeb76d7d0`, plage
`0x821B9710..0x821B97C7` et pseudocode SHA-256
`1a08f0a11ca831de2e87cdb31b5b6c6d60105856ba09d727b0e365e9de5b8492`.
Son corps généré qualifié appelle `0x821BE2E0`, `0x821C5090` et les deux
primitives de spinlock; il charge des champs relatifs à `r30`/`r13` et peut
faire un appel indirect, sans adresse littérale de frontbuffer. La partie
statique ne suffit pas à prouver le comportement de la cible indirecte; les
hooks dynamiques couvrent cette cible lorsqu’elle est exécutée.

Les callsites PAL des imports sont:

| import | callsite | fonction appelante |
|---|---|---|
| `VdSwap` | `0x821C5A1C` | `0x821C57D0` |
| `VdSetGraphicsInterruptCallback` | `0x821C645C`, `0x821C68C8` | `0x821C6400`, `0x821C6768` |
| `VdCallGraphicsNotificationRoutines` | `0x821C6CE8` | `0x821C6C98` |

## Résultats dynamiques A/B

| mesure | neutral 800 | START 800 |
|---|---:|---:|
| processus neuf | oui | oui |
| ticks complétés | 800 | 800 |
| `VdSwap`/PRESENT | 663 | 663 |
| callback enregistré | `0x821B9710`, contexte `0x10041A00`, tick 0/thread 1 | identique |
| appels callback | 800 | 800 |
| répartition source | 1× `source=1`, 799× `source=0` | identique |
| lectures scalaires frontbuffer | 0 | 0 |
| lectures vectorielles frontbuffer | 0 | 0 |
| guest writeback/digest | 1 / `0c660f2b…a4913a5f` | identique |
| frontend/mission/terminal | faux/faux/faux | faux/faux/faux |

Artefacts sous `TMPDIR`:

- neutral: RTPLY `bbc7ecb8fc20bfdbbdd1d70d24a4fdc78f6619dd5622c761e88541d776fd7068`,
  rapport `0d70fd74c24bd4205491c66c56bd4a5c31278dd99d6270a44248196990563ffa`,
  stderr callback `cb28c5410f943b63f95085e3d2fdc8de8d3b60f92512e1ae014b6e554011b985`;
- START: RTPLY `54a2860fccb21ab3be0595ae5532a7f9e0dbc3b7b971aaf334a7a087f5a427e1`,
  rapport `062e97c4087ec46b14979ecfd0ce3cef1e5f00531e27bfd899d7e6b7386c36fe`,
  stderr callback identique `cb28c5410f943b63f95085e3d2fdc8de8d3b60f92512e1ae014b6e554011b985`.

Les deux stdout annoncent `shader_loads=5`, `draws=26`, `presents=1`, un
resolve et le digest guest linéaire `0c660f2b…a4913a5f`. Les IB restent
`ef7ab6e4…d2b0` et `d121c8d8…358d6`.

## Garde et qualification

- `demo-qualified`: enregistrement et 800 invocations du callback exact,
  A/B byte-identiques, aucune lecture scalaire/vectorielle de la plage;
- `demo-observed`: callback `0x821B9710`, contexte, sources d’interruption,
  663 présentations et writeback guest;
- `xenia-generic`: aucune nouvelle preuve;
- `unknown`: cible de l’appel indirect interne, lecture scanout hôte,
  interprétation des champs callback, pixels non noirs, frontend, mission et
  screencap.

Le journal est opt-in (`AC6_DEMO_WATCH_GRAPHICS_INTERRUPT=1`), limité à
8192 appels, sans mutation de GuestMemory. Le produit normal conserve le hook
désactivé; aucun C++ généré, projet Ghidra, Xenia/ReXGlue, microcode ou actif
propriétaire n’a été modifié.

## Prochain checkpoint

Qualifier la cible indirecte appelée par `0x821B9710` (ou son premier
consumer de données) avec PC/LR/thread/tick, puis joindre un readback guest
non noir. Ne pas confondre les notifications d’interruption et la
consommation de pixels.


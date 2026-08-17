# Cycle 1705 — remplissage précédent du slot XMA

## Verdict

La seconde étape `reverse-continue` sur les traces `rr` neutral et START
rejoint le même writer précédent du mot `0x17360050` :

```text
0x821A3C30 + 0x000000A0 : bl 0x823273E0
callsite   0x821A3E70, bytes 48 18 35 71, LR 0x821A3E74
callee     0x823273E0, r4=0xFE, valeur observée 0xFEFEFEFE
```

Le callee est le helper PAL de remplissage octet par octet/mots. Il prépare la
zone avant le zero-fill `0x821A4B94` déjà qualifié au cycle 1704. Cette preuve
ne donne toujours pas le premier writer historique : une écriture antérieure
peut avoir été écrasée par le remplissage `0xFE`. Le tick et le thread de ce
store inverse n’ont pas été enregistrés par la sonde GDB; ils restent
explicitement inconnus.

## Identité et A/B

| élément | neutral | START |
|---|---|---|
| trace | `rr-neutral` | `rr-start` |
| frontier | tick 1048, ordinal 548 | tick 1048, ordinal 548 |
| thread/LR de l’import | 21 / `0x82357298` | 21 / `0x82357298` |
| slot | `0x17360050` | `0x17360050` |
| valeur du writer précédent | `0xFEFEFEFE` | `0xFEFEFEFE` |
| GDB watch SHA | `8bca938d…436391d` | `b296b0d2…9fb9a76` |

Les deux routes ont 911 `PRESENT`, les mêmes IB
`ef7ab6e4…d2b0`/`d121c8d8…358d6`, et aucune milestone frontend/mission/
terminal. XEX, basefile, rr et binaire sont ceux des cycles 1702–1704; les
hashes complets sont dans la capsule.

## Jointure statique

| frontière | taille | bytes SHA-256 | pseudocode SHA-256 |
|---|---:|---|---|
| `0x823273E0` (fill helper) | `0xA0` | `b89e717560e2a52f7975ff89a362a8301cab15e05c3d5f90dac72af83eadb91a` | `bd6f73382fa16ab22bab4895d3e3595564fbbec7c5d3753990e00d094db9ea08` |
| `0x821A3C30` (caller) | `0x9D0` | `bb41ae128e7548b79805f080e4953c9a65c81d0e0831509ccae29560ec9cfb3a` | `96a0dfe832598c2c8eff87663da17349f1cc6c235c6aee066d2e9b857cff5316` |
| `0x821A4B70` (zero-fill) | `0xFC` | `5c7d7d4282c7d5422c6a77015a1506f2fb8aa29ed6d8184acd7a5602f3a15d8b` | `858910269474b60e8a6c160921e3de37d795ba0d48830b5288a41eef69e53b27` |

Les bytes PAL au callsite `0x821A3E70` sont `48183571`; les bytes de l’entrée
du helper `0x823273E0` sont `38050001`. Le C++ généré n’est utilisé que comme
trace d’exécution; les frontières et hashes viennent de l’atlas PAL.

## Classification et garde

- `demo-qualified` : A/B rr, adresse/valeur du store, callsite et LR statiques,
  frontières/hashes PAL, chaîne fill→zero-fill→import.
- `demo-observed` : motif `0xFEFEFEFE` et l’ordre inverse des deux writes.
- `xenia-generic` : aucun élément.
- `unknown` : premier writer avant le fill, tick/thread du fill, table/index,
  retour/output pointer post-import, consumer, packets et PCM.

Le fill et le zero-fill sont des effets guest observés; ils ne constituent pas
une implémentation XMA. L’ordinal 548 continue de trap avant tout effet de
l’import. Aucun audio décodé, readback ou screencap n’est promu.

## Validation du projet

Après `export TMPDIR=/fastdata/lavaulta/tmp`, le CTest complet du build atomique
reste vert : **17/17**. La première invocation sans cette variable a été
refusée uniquement par la garde du test Vulkan (`TMPDIR must be
/fastdata/lavaulta/tmp`) et a été relancée correctement; aucun test produit
n’est en échec.

## Prochain checkpoint

Ajouter, sur une paire fraîche, une sonde read-only du store exact qui capture
TLS tick/thread et LR au moment du fill, puis chercher le premier consumer du
mot `[0x17360050,0x17360054)`. Tant que l’output pointer post-appel et le
premier paquet XMA ne sont pas démontrés, conserver le trap.

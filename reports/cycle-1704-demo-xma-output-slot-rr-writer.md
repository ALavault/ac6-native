# Cycle 1704 — writer `rr` du slot de sortie XMA

## Verdict

Deux traces `rr` fraîches (neutral et START) atteignent le même appel
`XMACreateContext` au tick 1048. Un watchpoint inverse sur le mot
`[0x17360050,0x17360054)` rejoint, dans les deux routes, le même zero-fill
PAL : fonction `0x821A4B70`, instruction `0x821A4B94`, bytes PAL
`7c205fec` (`dcbzl r0,r11`), appelée par `0x82356528` au callsite
`0x8235660c` (LR `0x82356610`, bytes `4be4e565`). Ce résultat ne provient pas
de `XMACreateContext`; l’import reste non implémenté et fail-closed.

Le watchpoint qualifie le dernier writer observé avant l’import, pas la
provenance complète de la table ni un ABI XMA. La valeur `0` vue à l’entrée est
donc un état pré-appel préparé par le guest; aucun mot de sortie post-appel
n’est disponible.

## Identité et protocole

| élément | valeur |
|---|---|
| cible | `Default.xex` démo PAL |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| basefile SHA-256 | `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| rr | `.tools/rr-install/bin/rr`, commit `7352eb807ed75e3b51be85fa6a27f121235dbfb0`, binaire SHA `33fd6e3eade957f5b0e4c7e12ddb9f6ff54ce522103ad418f1b6d14737f454d6` |
| binaire | `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp`, SHA `827dbc7b0ffce2918214c951320c118e79ab1b45c676574a60c5d6b0591edeb7` |
| fenêtre | max 1050 ticks, headless, stores neufs, import ordinal 548 |
| traces | `/fastdata/lavaulta/tmp/ac6-cycle1704.AkiNBj/rr-neutral` et `rr-start` |

Les événements `rr` ont les SHA `8fabf9189316f8178e5ad3a9cc9f6a2261791ff5bc87eeb5e9abedf1b48ab065`
(neutral) et `69b647db1702927f219fee9ffa3630088d3c48117e42a831471dfd41d5366afb`
(START). Les traces restent sous `TMPDIR` et ne sont pas suivies dans le
projet.

## A/B dynamique

| route | tick/import | thread/LR | `r3` | fenêtre pré-appel SHA | RTPLY SHA | rapport SHA |
|---|---:|---|---|---|---|---|
| neutral | 1048 / ordinal 548 | 21 / `0x82357298` | `0x17360050` | `a4f014e0…6dedf` | `6a759832…8b428f20` | `7c2555e8…f3ae131` |
| START | 1048 / ordinal 548 | 21 / `0x82357298` | `0x17360050` | `a4f014e0…6dedf` | `d53bf82d…2268c8d` | `238bd9e4…34022cc` |

Chaque route compte 911 notifications `PRESENT`, les mêmes IB
`ef7ab6e4…d2b0`/`d121c8d8…358d6`, et aucune milestone frontend/mission/terminal.

## Watchpoint inverse

Le mot surveillé est l’adresse guest `0x17360050`; les adresses hôte sont
`0x797c63360050` (neutral) et `0x705ffe760050` (START). Dans les deux traces,
`reverse-continue` s’arrête dans `__memset_avx2_unaligned_erms` pendant un
`memset(..., 0, 0x80)` émis par `sub_821A4B70`; la pile hôte immédiate est :

```text
__memset_avx2_unaligned_erms
  __imp__sub_821A4B70
  __imp__sub_82356528
  __imp__sub_82351778
  __imp__sub_82353820
```

Le retour hôte est à l’offset `+0x19c` de `__imp__sub_821A4B70`, cohérent avec
le bloc généré pour `dcbzl r0,r11` à `0x821A4B94`. La trace GDB montre, dans
les deux routes, `Old value = 0`, `New value = 0xfefefefe` en marche inverse;
cela correspond à un zero-fill antérieur à l’état courant, et non à une
écriture de l’import.

Preuve statique de la chaîne :

| fonction | taille | bytes SHA-256 | pseudocode SHA-256 |
|---|---:|---|---|
| `0x821A4B70` | `0xFC` | `5c7d7d4282c7d5422c6a77015a1506f2fb8aa29ed6d8184acd7a5602f3a15d8b` | `858910269474b60e8a6c160921e3de37d795ba0d48830b5288a41eef69e53b27` |
| `0x82356528` | `0x264` | `89f43ea2bddb86eaf02618a1ffc86f70924b5c01c49cf7b55ac56d92dce76e3a` | `d4754a9723b0f651b24ad995ca60fcee1874e7bd79e1189eddfa20a2c095dad5` |

Au callsite `0x8235660c`, les bytes PAL sont `4be4e565` et le code appelle
`0x821A4B70`; cette jointure utilise le basefile PAL et la frontière Ghidra,
pas le nom du C++ généré.

## Classification et garde

- `demo-qualified` : identité XEX/basefile, A/B rr, import/tick/thread/LR/r3,
  dernier writer et bytes du callsite/fonction.
- `demo-observed` : zero-fill de 128 octets et motif `0xfefefefe` rapporté par
  le watchpoint inverse; le motif n’est pas promu comme champ XMA.
- `xenia-generic` : aucun élément nouveau utilisé.
- `unknown` : premier writer historique, index/table exacts, valeur de retour,
  output pointer post-appel, champs/paquets/timestamps/volume et PCM.

La garde conserve le trap sur ordinal 548. Aucun service XMA, décodage
FFmpeg/vgmstream, readback ou screencap n’est activé.

## Prochain checkpoint minimal

Instrumenter uniquement les stores guest de la plage `[0x17360050,0x17360054)`
avant l’appel, avec PC/LR/thread/tick, en partant d’une nouvelle paire de
traces. Puis qualifier le premier consumer de ce mot. Toute écriture ou retour
post-appel reste interdite tant que l’ABI et un paquet XMA PAL exacts ne sont
pas démontrés.


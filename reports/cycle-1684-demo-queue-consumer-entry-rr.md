# Cycle 1684 — entrée du consumer de file sous `rr` (corrigé)

> Erratum ABI : la première sonde indexait `r3` à `ctx+24`, alors que
> `PPCContext` place `r3` à `ctx+0`. Les valeurs ci-dessous ont été régénérées
> avec la table corrigée ; l’ancienne lecture est supersédée.

## Verdict

La sonde inverse read-only rejoint l’entrée recompilée `0x820FEFA8` avec le
worker guest `0x820FFCA0` sur la trace START historique qualifiée : 16 appels
consécutifs, ticks 252 à 267, thread 25, LR guest `0x820FFD8C`, r3
`0x2EEEBE90`, et r31
`0x82386CC0`. Le point d’arrêt est le stub hôte
`__imp__sub_820FEFA8` (PC hôte `0x53217Bcb` dans cette exécution), pas un PC
guest ; le LR et la callsite guest sont donc les éléments d’adresse retenus.

La sonde brute corrigée sur les deux traces `rr` process-fresh du cycle 1680
(même binaire noinline, neutral et START) produit un appel identique au tick
252 dans chaque route. Cela ferme l’A/B de cette frontière, mais ne remplace
pas une qualification sémantique du consumer.

## Identité et méthode

| élément | valeur |
|---|---|
| cible | `Default.xex` PAL démo, SHA-256 `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| architecture | Xenon big-endian / Xenos |
| basefile jointe | `.build/ac6-demo-codegen-xenon-38/xex-basefile.bin`, SHA `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| rr | `.tools/rr-install/bin/rr`, source `7352eb807ed75e3b51be85fa6a27f121235dbfb0`, binaire SHA `33fd6e3eade957f5b0e4c7e12ddb9f6ff54ce522103ad418f1b6d14737f454d6` |
| sonde | `tools/rr_queue_consumer_entry.gdb`, SHA `ae83abd9432690038fe9902fd518794bde732cc2400973ed1096adfdc3c6d683` |
| route principale | START movie headless, trace `/fastdata/lavaulta/tmp/ac6-demo-start-rr-t300.B10dbp/trace`, terminal tick 300 |
| binaire de cette trace | SHA `aa2345b8eb96aa19bf601b25698e8faff3886ece1b84829539fc4eb1e6fb6a0f` |
| log de la sonde | `/fastdata/lavaulta/tmp/ac6-queue-consumer-entry-gdb-corrected.log`, SHA `579611b358fb808c09a5245323ef93ad6d88ca2757f58ec46cb52ce3e446c424` |

La sonde lit uniquement le contexte GDB, le tick/TLS, le thread, les GPR et
des champs guest bornés ; elle n’écrit ni mémoire guest ni état runtime. La
trace a été parcourue avec `rr replay --serve-files` et GDB remote ; la fin
`SIGKILL` est celle du replay historique borné, pas une mutation du runtime.

## Observations dynamiques

| ticks | thread | LR guest | r3 | r4 | r5 | r6 | r7 | r31 |
|---|---:|---:|---:|---:|---:|---:|---:|---:|
| 252–267 (16 appels consécutifs) | 25 | `0x820FFD8C` | `0x2EEEBE90` | `0x00000001` | `0x00000000` | `0x00000000` | `0x00000030` | `0x82386CC0` |

Les champs testés aux offsets `0,4,8,0x14,0x18,0x40,0x44,0x48,0x4c,0x50`
sont lisibles et valent tous zéro dans les 16 entrées. L’objet de travail est
non nul, mais son payload observé reste entièrement nul.

La jointure statique exacte, dans `sub_820FFCA0`, est :

```text
addi r3,r1,80
bl   0x820FEFA8
LR   0x820FFD8C
```

Les bytes et pseudocodes PAL déjà scellés sont :

| fonction | bytes SHA-256 | pseudocode SHA-256 | taille |
|---|---|---|---:|
| `0x820FEFA8` | `652a3a166707a01b22bf62e2cb3391ec471357a721ee2d46ccd6932af507edd9` | `66172ed7f35ef941abb17efbb0eb6dd3960717560017a29f05f58bddffc10b0d` | `0x6ec` |
| `0x820FFCA0` | `efe72ffa05bb2cd3ad991e1f063b7c9d94ad91f7c4808da133849dd9fd7ee119` | `5170446450d957db1e07a92689aeb1d89884f39dba8f827c0fe97133750a6b24` | `0x110` |

La deuxième ligne de pseudocode est volontairement laissée comme preuve de
callsite ; le rôle métier des deux fonctions reste `unknown`.

## A/B et qualification

Les rapports cycle 1680 et 1683 établissent séparément que neutral et START
partagent le binaire PAL qualifié, le scheduler borné, les IB
`ef7ab6e4…d2b0`/`d121c8d8…358d6`, les PRESENT et le `XE_SWAP`. La sonde
brute corrigée du cycle 1685 confirme maintenant un appel au tick 252 dans
chaque route ; la trace START t300 fournit en plus les 16 appels successifs.

- `demo-qualified` : identité XEX/basefile, bytes des fonctions, callsite
  `0x820FFD8C`, thread/ticks/LR de la trace START, et lecture bornée sans
  effet.
- `demo-observed` : 16 entrées de `0x820FEFA8` dans la trace START ; objet
  stack non nul et champs bornés tous nuls.
- `xenia-generic` : aucun élément utilisé.
- `unknown` : rôle sémantique, consommation du frontbuffer,
  différence neutral/START au niveau de cette entrée, pixels et transition
  frontend.

## Garde ciblée et prochain checkpoint

La garde durable est : ne promouvoir cette frontière que si une nouvelle
trace neutral et une trace START du même runtime/codegen produisent, dans les
mêmes fenêtres, des lignes avec `thread=25`, LR `0x820FFD8C`, r3 non nul, r31
`0x82386CC0`, et des champs lisibles ; toute divergence ou champ inaccessible
reste un résultat `unknown` et arrête le corridor avant tout effet visuel.

Le prochain test minimal est une paire `rr record` process-fresh à tick 300
avec cette sonde installée dès le démarrage, puis une fenêtre de watchpoint
exacte sur le slot `[0x82386D90,0x82386DD0+96)` uniquement si r3 devient non
nul. Aucun writeback synthétique, aucune resynchronisation de trace, aucun
fallback visuel et aucune fusion retail ne sont autorisés.

Politique : aucun checkout Xenia/ReXGlue/Ghidra, C++ généré, microcode ou
actif propriétaire modifié ou suivi.

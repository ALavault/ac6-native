# Cycle 1752 — état body-side neutral après les six kicks XMA

Une sonde read-only opt-in (`AC6_DEMO_WATCH_BODY_STATE=1` et
`AC6_DEMO_WATCH_INDIRECT_OBJECT=1`) a rejoué neutral jusqu'à tick 5400 avec
les six tuples XMA déjà qualifiés. Elle n'écrit ni objet, ni event, ni registre
MMIO : elle journalise seulement l'appel indirect et les stores bornés dans
`[0x82934000,0x82935000)`.

Résultat : 5153 activations de `LR=0x822E559C -> 0x822F8848`, toutes avec
`object=0x82934280`, `vtable=0x8202A488`, slot 4 mappé et cible
`0x822F8848`. Les registres observés sont stables (`r4=0`, `r5=0x57C`,
`r6=0x100`, `r7=0`). Chaque activation produit exactement dix stores, soit
51 530 lignes. Les dix couples adresse/valeur sont constants :

```text
0x829341A4 = 0x829342A0
0x829341A8 = 0
0x829341AC = 0
0x829341B0 = 0
0x829341B4 = 0x82934500
0x829341B8 = 0x829342A0
0x829341BC = 0
0x829341C0 = 0
0x829341C4 = 0
0x829341C8 = 0x82934500
```

Les premiers hits sont au tick 0/1/222 et les derniers aux ticks 5397–5399;
la stabilité persiste après les six kicks (`1/2/4` au tick 1048, `8/16/32` au
tick 5052). Le run atteint 5263 PRESENT, `frontend=false`, `mission=false`,
`terminal=false`, et 23/23 threads bloqués sur `0x822E559C -> 0x822F8848`.

Cette capsule ferme seulement la classification **demo-qualified** de l'état
objet borné. Elle ne qualifie ni le rôle métier de la fonction, ni un writer
EDRAM, ni un pixel, ni l'effet XMA/audio. Le prochain test reste un watchpoint
guest exact sur les plages mémoire susceptibles d'alimenter `RB_COPY`, sans
ouvrir START.

Artefacts :

- rapport : `/fastdata/lavaulta/tmp/ac6-cycle1752-body.t7XYSI/neutral.report.json`, SHA-256 `ec40217ffd824cd7e6f525497b31a05d4c6ca7e14bfdf2746e8de6fc3ce93a44` ;
- trace RTPLY-v4 : `/fastdata/lavaulta/tmp/ac6-cycle1752-body.t7XYSI/neutral.trace.jsonl`, SHA-256 `793f57e338aa7265f1e4d3c06d0a6d9865fc34da37e0ec9abee1653d6a5c1d6e` ;
- stderr body : SHA-256 `4ac886e87e0236566d3b72aeb9ab5d49ec33b20debfd55bb003dc02bb052858c` ;
- binaire codegen-ON : SHA-256 `b31b5f4346b2f50eea40a74ab43b10f2502c1b49ee72dcfafffaf2aaca1d8ccb`.

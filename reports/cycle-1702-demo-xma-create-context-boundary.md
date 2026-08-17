# Cycle 1702 — premier `XMACreateContext` démo PAL

## Verdict

Après le shim strict du thunk START de la piste précédente, deux exécutions
fraîches (neutral et START) dépassent le frontier tick 268 et convergent au
tick 1048 sur l’import `xboxkrnl.exe:XMACreateContext` (ordinal 548). La
capture est identique dans les deux routes et ne qualifie encore ni le
décodeur XMA, ni un paquet, ni une sortie audio. L’import reste donc
fail-closed; aucune donnée synthétique n’est injectée dans le guest.

## Identité et protocole

| élément | valeur |
|---|---|
| cible | `Default.xex` PAL démo |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| architecture | Xenon big-endian / Xenos |
| basefile SHA-256 | `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| binaire | `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp` |
| binaire SHA-256 | `827dbc7b0ffce2918214c951320c118e79ab1b45c676574a60c5d6b0591edeb7` |
| sonde | `AC6_DEMO_WATCH_XMA_CREATE=1`, désactivée par défaut |
| source sonde SHA-256 | `2e0ff0efa733bba832179110452a87bd98457a361db8d599eaaaf7388fdc4ade` |
| fenêtre | `max_ticks=1050`, backend headless, stores et processus neufs |
| START | bouton `0x10` aux ticks 252–267 |

Les médias restent uniquement dans les stores locaux de test; aucun contenu
audio décodé, microcode, shader ou autre actif propriétaire n’est copié dans
le projet.

## Résultat A/B

| route | ticks accomplis | PRESENT | frontier | RTPLY SHA-256 | rapport SHA-256 | stderr SHA-256 |
|---|---:|---:|---|---|---|---|
| neutral | 1048 | 911 | import ordinal 548 | `6a759832c5471405591dac956bbc96dd99ab087d274067d01eea18849b428f20` | `7c2555e8ada7d3c5b65525ef57024fc6af404a3224f4d65e832b1ed1fe3ae131` | `9fa701c6a30b712e96eb94dfc014db565d7928dd3c2b7afe77f9d16de3547c31` |
| START | 1048 | 911 | import ordinal 548 | `d53bf82d50724f6b0d771f30edfcdc17439b63fabba6a1e884b72a1ac2268c8d` | `238bd9e4f7e8ea150b7e5622101340145bc0dd9e7076d5c6472f4970634022cc` | `9fa701c6a30b712e96eb94dfc014db565d7928dd3c2b7afe77f9d16de3547c31` |

Les deux rapports indiquent `frontend=false`, `mission=false` et
`terminal=false`. Le premier run à 1200 ticks (cycle 1701) confirme que ce
frontier est reproductible; il n’a pas été contourné.

## Preuve dynamique PAL

Au callsite de retour `LR=0x82357298`, thread guest 21, tick 1048 :

```text
AC6_XMA_CREATE tick=1048 thread=21 lr=0x82357298 r3=0x17360050
  r4=0x00000000 r5=0x00006180 r6=0x00000000 r7=0x00000001
AC6_XMA_CREATE_DESCRIPTOR mapped=1 address=0x17360050
  00000000 17360180 17361F80 00000000 00000000 00000000
  00000000 00000000 07800000 B8800000 00000000 00000000
  00000000 00000000 00000000 17362180 17363F80 00000000
  00000000 00000000 00000000 00000000 00000000 00000000
```

Les 96 octets guest, dans l’ordre Xenon big-endian, ont le SHA-256
`a4f014e03e752249dc9740522458b6fc4d212d013add1e7f2a1d99cc3346dedf`.
La même adresse et les mêmes 24 mots sont observées dans neutral et START.

La capture prouve seulement que `r3=0x17360050` est un descripteur mappé et
que l’appel est atteint. Elle ne donne pas encore le format sémantique de ses
champs ni l’ABI complète de l’objet XMA.

## Jointure statique

Le manifest d’import généré depuis le XEX qualifié nomme l’ordinal 548
`XMACreateContext`. Le callsite est dans la fonction PAL
`0x82357240..0x8235730B` (taille `0xCC`, bytes SHA-256
`7436f8404267283916f2f2e64fdcda534788553fbf366daa330bc09fe9220ed9`). Les
bytes PAL au site `0x82357294` sont `48 01 F5 21 7C 77 1B 78`; l’instruction
suivante fixe le retour `LR=0x82357298`. Le code statique passe
`r31 = base + 64 + index * 96` à l’import, puis relit `0(r31)` après le
retour et parcourt les entrées avec une stride de 96 octets. Cela qualifie la
géométrie de la table, pas le nom des champs audio.

## Référence générique séparée

Le checkout Xenia local est `tools/xenia-source`, commit
`95a5c3ee250f80c3b9d139658649d9ffb6db3eec`; le fichier
`src/xenia/kernel/xboxkrnl/xboxkrnl_audio_xma.cc` a le SHA-256
`73c92f8c5196694f838a6f787750681e3f7a4cd044b478c118c3035a46e3c86c`.
Il décrit génériquement un appel recevant un pointeur de sortie et retournant
un contexte, mais cette information reste `xenia-generic` et n’est pas une
preuve de l’ABI AC6. Aucun fichier Xenia n’a été modifié.

## Classification et garde

- `demo-qualified` : identité XEX/basefile, import ordinal 548, callsite et
  bytes PAL, tick/thread/registre, adresse et contenu borné du descripteur,
  convergence neutral/START et absence de jalon visuel.
- `demo-observed` : répétition de la même table 96 octets dans les deux
  routes et progression à 911 PRESENT.
- `xenia-generic` : nom/prototype indicatif de Xenia uniquement.
- `unknown` : valeur de retour, allocation du contexte, champs du descripteur,
  registres XMA, packets/timestamps/volume, langue, décodage FFmpeg/vgmstream,
  PCM et consommation SDL3.

La sonde est read-only et opt-in. Le hook ne modifie aucun registre ou octet
guest. La garde conserve le trap sur ordinal 548; aucune promotion de START,
readback ou screencap n’est autorisée par ce cycle.

## Validation

- build codegen-ON incrémental réussi;
- `SDL_AUDIODRIVER=dummy xvfb-run -a ctest --test-dir .build/ac6-demo-atomic-runtime-1 --output-on-failure` : **17/17**;
- audit de complexité : **pass**, 90 fichiers;
- audit source et statut : **pass**;
- `spirv-val` et `vgmstream-cli` non invoqués : aucun SPIR-V ou paquet XMA
  atteint par cette frontière.

## Prochain checkpoint

Ajouter seulement une capture bornée du mot écrit par `XMACreateContext` dans
`[0x17360050,0x17360054)`, puis des premiers reads/writes de la même entrée,
sur neutral/START frais. Toute implémentation doit attendre une égalité
deterministe de cet output pointer, la qualification des appels XMA suivants
ou des registres MMIO et l’identification exacte d’un paquet avant d’appeler
FFmpeg/vgmstream. Le trap reste obligatoire si une de ces preuves manque.

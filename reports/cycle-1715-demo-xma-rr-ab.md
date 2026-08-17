# Cycle 1715 — A/B `rr` de la frontière XMA expérimentale

## Verdict

Le gate local `rr` est étendu de façon bornée à l'expérience XMA opt-in,
neutral et START, sans modifier la route par défaut. Pour chaque route, le
RTPLY, le rapport de frontière et le stderr sont byte-identiques entre
l'exécution directe et `rr`. Les deux routes atteignent le même trap avant
effet à `0x7FEA1A80`, tick 1048, thread 21, après 911 notifications PRESENT.

Cela qualifie la fidélité d'ordonnancement de cette expérience, pas la
sémantique du registre XMA, les packets, l'audio ou le renderer. Le registre
reste inconnu et l'expérience ne devient ni `play` ni `replay`.

## Identité et outils

| élément | valeur |
|---|---|
| cible | `Default.xex`, démo PAL, Xenon big-endian/Xenos |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| basefile SHA-256 | `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| binaire codegen ON | `.build/ac6-demo-atomic-runtime-1/ac6-demo-recomp` |
| binaire SHA-256 | `d6ad26517ed9e746cf50b0a499db924daca5cade4f8129708ee213c5acde49d3` |
| `rr` | `/fastdata/lavaulta/auto-re-agent/.tools/rr-install/bin/rr` |
| `rr` source commit | `7352eb807ed75e3b51be85fa6a27f121235dbfb0` |
| `rr` binaire SHA-256 | `33fd6e3eade957f5b0e4c7e12ddb9f6ff54ce522103ad418f1b6d14737f454d6` |
| borne | `probe`, headless, `max_ticks=1050`, `AC6_DEMO_EXPERIMENTAL_XMA_CREATE=1` |

Les stores sont ceux du A/B cycle 1712 :
`/fastdata/lavaulta/tmp/ac6-cycle1712-ab/neutral-store` et
`/fastdata/lavaulta/tmp/ac6-cycle1712-ab/start-store`. Les traces `rr` et
sorties complètes restent sous `/fastdata/lavaulta/tmp` et ne sont pas
versionnées.

## Comparaison directe / `rr`

| route | direct RTPLY | `rr` RTPLY | direct rapport | `rr` rapport | stderr commun |
|---|---|---|---|---|---|
| neutral | `ab54c75e…804fc43` | identique | `e4c33682…eaf399e0` | identique | `18b1a2b6…008bd7b` |
| START (`0x10` au tick 252) | `2ce0a717…7db417f` | identique | `54ba6698…bc490e8` | identique | `18b1a2b6…008bd7b` |

Chaque rapport contient `completed_ticks=1048`, `presentation_notifications=911`
et le même frontier :

```text
thread=21
lr=0x823572AC
address=0x7FEA1A80
trap_before_effect=true
```

Le RTPLY neutral/START diffère comme attendu à cause de l'événement START;
la comparaison pertinente est directe contre `rr` pour une même route. Les
traces ont été enregistrées dans des répertoires distincts avec le `rr`
épinglé et un processus frais.

## Classification

- **demo-qualified** : identité PAL, hash du binaire, A/B direct-vers-`rr`,
  égalité du frontier/tick/thread/LR/adresse, 911 PRESENT.
- **demo-observed** : allocation expérimentale `0x2E800000`, contexte écrit
  dans `0x17360050`, tentative de store `0x7FEA1A80`, trap avant effet.
- **xenia-generic** : aucune nouvelle sémantique importée; le registre n'est
  pas nommé depuis Xenia/ReXGlue.
- **unknown** : effet de `0x7FEA1A80`, base/indexation matérielle XMA, packets,
  timestamps, volume, PCM, langues et pixels.

## Garde et prochain checkpoint

La route par défaut conserve le trap ordinal 548 et n'est pas activée par
`play`/`replay`. Ne pas mapper `0x7FEA1A80` par approximation. Le prochain
test ciblé est une sonde PAL read-only autour de `FUN_82356510` et
`Function_82357240` : relever la valeur lue à `0x7FEA1800`, le global
`0x829DA52C`, puis l'adresse formée au `stwbrx` `0x823572D8`, neutral et START,
avec PC/LR/thread/tick. En cas de valeur non jointe, conserver le fail-closed.

Capsule : `analysis/demo/ac6-demo-xma-rr-ab-v1.json`.

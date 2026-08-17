# Cycle 1603 — Xenia Linux ORACLE_RECOVERY bornée

```text
ROLE=ORACLE_RECOVERY
LANE=stock (Xenia oracle uniquement; aucune promotion en stock_observed de la logique du jeu)
TARGET=default.xex
TARGET_SHA256=acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde
IMAGE_BASE=0x82000000
DATA_TBL_SHA256=82700410d305dc2d24e24d378ce5b9b63f240ac208842d7620b608fac15d50f5
```

## Version et périmètre

Le checkout Xenia `/fastdata/lavaulta/auto-re-agent/tools/xenia-source` est
propre à `95a5c3ee250f80c3b9d139658649d9ffb6db3eec`; après `fetch --prune`,
`origin/master` est identique. Aucune mise à jour n'était donc nécessaire.
Le binaire Canary utilisé est
`/tmp/ac6-xenia-runs.9E8fSW/build/bin/Linux/Release/xenia_canary`, SHA-256
`98559834c570d4be8ba5d532f000aadf8ea6cf4d495be34a02b7ae766134007c`.
La campagne n'a modifié ni Xenia source ni AC6 et n'a utilisé ni Wine ni
réparation Xenia.

## Matrice bornée

| Variante | Dernier jalon observé | Premier défaut / observable |
|---|---|---|
| A — Vulkan, `SDL_AUDIODRIVER=dummy` | swapchain, client audio, bind réseau, `KernelState: Launching module` | screenshots 8 s / 20 s noires et identiques, SHA-256 `42fa11d06dad4ca1a1793a84882aa8a1fcb6eaeeb7373dbec443d2150e448a6b` |
| B — `apu=nop` | lancement du module | `AudioSystem::RegisterClient: CreateDriver failed for index=0` |
| C — `gpu=null`, `apu=nop` | lancement du module | même défaut APU; `gpu=null` ne qualifie aucune progression CPU |
| D — `break_on_start=true`, Vulkan, `apu=nop` | entrée du mode debugger | aucun PC guest lisible sans attacher l'UI debugger locale; arrêt de la campagne |

Les quatre runs indiquent `ProfileManager: Found 0 Profiles`. Les SHA-256 des
logs conservés temporairement sont, dans l'ordre A/B/C/D :

```text
52edd94e53e93398e7acd9cce4625be6328543c0d862e3db8aa0898d196a7300
1896acb7a61210b8f819f165063905c4599851bc20268908c0719b9333a20e01
206042ede8681938743d262c3fdd6d21570697a0ed72fda4e729903c85e39f41
255f9744b8198cae3d70c7cc8a4a39fe26aa3b45fd025806e6f98afc153326f2
```

## Réconciliation avec la campagne profilée précédente

`ProfileManager: Found 0 Profiles` est un défaut de précondition de cette
matrice : la configuration temporaire ne contenait pas le profil jetable
utilisé par la campagne Linux du cycle 1540. Cette ligne explique pourquoi
ces quatre runs ne peuvent pas qualifier le chemin utilisateur, mais elle ne
ferme pas la cause du noir. Le cycle 1540 a chargé un profil jetable en slot 0
sur le même binaire `02d2cb5`, puis sur `e31142bd`; les deux ont créé les fils
invités, enregistré le client audio avec `SDL_AUDIODRIVER=dummy` et effectué le
bind, tout en produisant deux captures 8/20 s identiques et noires.

La variante `apu=nop` est donc un contrôle de panne audio uniquement. Son
`CreateDriver failed` est attendu avec ce backend et ne constitue pas une
preuve de blocage CPU. La prochaine campagne Xenia devrait toujours fournir
un profil isolé valide avant toute conclusion, puis sonder la frontière entre
le bind et le premier `PRESENT`/submit GPU.

## Conclusion qualifiée

`stock_observed` est limité aux jalons hôte explicitement journalisés
(chargement, swapchain/audio du run A et erreurs APU des runs B/C). Il n'y a
aucune observation `stock_observed` du frontend, du CPU guest, du renderer
Xenos ou de l'audio de la démo. Le profil absent est un prérequis bloquant
pour cette matrice, mais la campagne profilée précédente montre que le
blocage substantiel reste la progression Linux/Xvfb après le lancement et
avant un `PRESENT` de contenu. L'écran noir ne constitue pas une preuve
d'absence de progression guest et ne justifie pas de modifier le produit
native.

Dette d'oracle ouverte : qualifier, dans une campagne séparée, le service de
profils et le premier PC guest atteint avec le même XEX, puis distinguer
progression CPU et présentation. Scénario minimal : profil neutre créé par le
chemin qualifié, `SDL_AUDIODRIVER=dummy`, un boot jusqu'au premier `PRESENT`,
artefact attendu : PC guest/thread, log de service et hash de capture. Cette
dette ne bloque pas les corridors static/bridge/native indépendants.

Les logs et captures restent sous `/tmp/ac6-xenia-runs.9E8fSW/` et ne sont pas
versionnés. Aucun processus Xenia ou Xvfb ne reste actif.

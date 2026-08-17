# AC6 `ac6-demo-native` — journal replay cycle 1766

Verdict : **REPLAY-CONTRACT-GO / RUNTIME-NO-GO**, `supported=false`.

Le commit `985ff6b52a163b2105a6e232ce9715a4dbfe23b8` ajoute un journal
canonique `AC6RTPLY-v4` en mémoire pour la frontière plateforme cycle 1765.
Chaque record porte action XInput bornée et observation tick/PRESENT/temps. Le
reader exige l’identité PAL démo, l’ordre contigu, les bornes, le trailer
SHA-256, puis rejoue chaque record par le même `PlatformRuntime`.

Deux sérialisations d’une route replayée sont byte-identiques. Une action
malformée, l’identité retail et l’absence de trailer sont rejetées avant toute
promotion. Le journal est borné à 1 000 000 records, 256 MiB et 512 octets par
record canonique.

```text
cmake --build ... -j16                                          PASS
SDL_AUDIODRIVER=dummy xvfb-run -a ctest                         5/5 PASS
ac6-demo-native-replay: record/replay/reserialize               PASS
complexity audit                                                PASS (17 fichiers)
```

Le format est compatible avec les champs d’entrée `AC6RTPLY-v4`, mais cette
preuve reste in-memory et plateforme-only. Aucun fichier replay CLI, socket
MCP `demo-native`, guest scheduler, readback, frontend, mission ou terminal
n’est exposé. Aucune parité runtime recomp/native n’est revendiquée.

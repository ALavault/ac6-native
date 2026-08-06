# Cycle 811 — runner de replay intégré

`MissionRuntime::run_replay` rejoue chaque `InputFrame` du `ReplayLog` via le
même scheduler fixe et retourne la dernière frame. Deux runtimes exécutant la
même séquence produisent des ticks et transforms identiques.

Validation : build CMake et CTest `1/1` réussi.

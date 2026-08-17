# Cycle 1605 — évaluation du debugger Xenia Linux comme oracle

```text
ROLE=ORACLE_RECOVERY
LANE=stock avec intervention hôte déclarée
TARGET_SHA256=acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde
XENIA=canary 907d92b
INTERVENTION=strace -f -qq -e trace=none
QUESTION=obtenir un PC PPC, un thread guest et ses registres puis reprendre
```

## Protocole borné

Deux copies temporaires indépendantes du bundle portable qualifié ont été
exécutées sous Xvfb, Vulkan et `SDL_AUDIODRIVER=dummy`, avec `--debug=true`,
`--store_all_context_values=true` et `--log_level=3` :

1. arrêt immédiat avec `--break_on_start=true` ;
2. boot visuel, puis une commande manuelle unique
   `CPU -> Break and Show Guest Debugger`.

Aucune source Xenia ou AC6 n'a été modifiée. Le Xvfb partagé d'AC5 n'a pas été
touché.

## Résultats

Le premier run atteint `KernelState: Launching module` puis journalise
`Breaking into debugger because of --break_on_start`. Il s'arrête avant la
création du premier thread guest et n'expose ni PC PPC ni registres.

Le second run charge le profil, crée 31 threads et progresse visuellement dans
la cinématique. La commande debugger ouvre une boîte `Xenia Debugger` qui
indique que le mode debug n'est pas actif. La cause est antérieure et exacte
dans les deux logs :

```text
Stack walker unimplemented on posix
Disabling --debug due to lack of stack walker
```

Le checkout courant confirme que
`src/xenia/cpu/stack_walker_posix.cc::StackWalker::Create` retourne toujours
`nullptr`. `Processor::Initialize` annule alors `cvars::debug`; le panneau
threads/registres ne peut pas être créé.

Artefacts temporaires :

```text
run break-on-start log  dde02a8248be8c388e36e6d3093dcd8d573dcd5c5facfa9e1edb54255d8fe13c
run live log            bfab2072166ee6bd766e556e78a860fb6503eecfe0f8027acf4fe62d209d5b24
capture live            e9496b01b9413fa6d676adbae7531b48284d40eb750632425c932829f2466ffb
capture erreur debug    2204ac9cb88192087bbea06fa2ff250bd164be16ea5b62d851e1c87f51a7ca99
```

Les logs, captures, profil et sauvegardes restent sous
`/tmp/ac6-xenia-debug-oracle.eYjzxS/` et
`/tmp/ac6-xenia-debug-live.h420Cq/`. Aucun processus de cette campagne ne reste
actif.

## Conclusion

Le Canary Linux est désormais un oracle **visuel** utilisable sous
l'intervention `strace`, mais pas un oracle **debug guest** : il ne peut pas
répondre actuellement à « quel PC/thread/registres ? ». Aucun fait PPC n'est
donc promu `stock_observed` par cette campagne.

La première divergence est le stack walker POSIX absent. La correction
minimale suivante appartient à une campagne Xenia séparée : implémenter ce
stack walker ou découpler le debugger invité de sa disponibilité, puis répéter
exactement l'arrêt unique. Cela ne bloque pas les campagnes `static`, `bridge`
ou `native`, qui restent les outils de conversion quotidiens.

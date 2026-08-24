# Cycle 1797 — premier effet enfant réduit au sous-enfant

Le run naturel conservé montre que l'enfant post-START `0x2E3F8C50` n'exécute
aucun offset VM lors de son premier tick. Les seuls `execute_raw` du tick 3001
appartiennent au parent `0x2E3EDA90`.

Le premier effet observé de l'enfant est la construction immédiate du
sous-enfant `0x2E3F1350` (`MovieMemory=0x2E3F1590`). La piste d'un callback VM
direct depuis la première frame de `0x2E3F8C50` est donc réfutée pour ce tick.

La prochaine observation doit corréler la seconde factory, son `B/D`, la
frame 0 du sous-enfant et son premier effet sortant. Fenêtre : tick 3001.

`frontend=false`, `mission=false`, `terminal=false`, `supported=false`.


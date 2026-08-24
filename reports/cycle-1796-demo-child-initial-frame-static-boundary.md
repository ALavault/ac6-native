# Cycle 1796 — frontière statique de la première frame enfant

Le gate qualifie la chaîne complète allant de la sélection dynamique du
descripteur enfant à son premier tick récursif. La frame 0 est sélectionnée
naturellement et tout élément type 6 éventuel est drainé vers `0x82325288`
pendant ce tick.

Il réfute toutefois l'attribution du pointeur runtime `D=0x2DD7936C` à un
descripteur constant inventorié dans BrandLogo : `D` provient de la table
relocalisée propre au storage Title. Les captures existantes omettent les mots
de `B`, de la commande source et de `D` nécessaires pour retrouver la frame.

La nouvelle frontière est une observation guest-state unique au tick 3001,
de l'entrée de `0x82323468` au retour du premier tick enfant. Elle doit joindre
la commande, `D`, frame 0, ses éléments et les offsets remis à `0x82325288`.

Statuts : `frontend=false`, `mission=false`, `terminal=false`,
`supported=false`.


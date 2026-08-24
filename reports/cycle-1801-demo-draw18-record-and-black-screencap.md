# Cycle 1801 — le draw 0x12 crée bien ses records, la sortie reste noire

Le chemin naturel du sous-enfant SWG ne s'arrête ni au handle ni au
validateur de payload. Pour les seize records `list13`, l'index `0x12`
sélectionne le handle valide `0x0E000071`, passe `0x820EA9A0`, soumet le même
payload par `0x82095DF0`, puis crée un record renderer de 0x70 octets par
`0x821DEED8`.

Le framebuffer courant a été vérifié indépendamment : 21 captures Xvfb sont
toutes uniformément noires, et `capture-020.png` a été inspectée humainement.
Le renderer rapporte toujours 24 draws typés mais aucune présentation.

La frontière statique exacte est donc le consumer du record produit, puis son
éventuel encodage PM4/Xenos. La recherche en amont dans `MovieController`, la
liste 13 ou la table de handles n'est plus discriminante.

Preuve complète :
`artifacts/goal-playable/title-draw18-record-runtime-20260824/RESULT.md`.

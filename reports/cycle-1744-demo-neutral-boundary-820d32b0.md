# Cycle 1744 — frontière neutral `0x820D32B0` fermée

La cible indirecte `0x820D32B0`, atteinte depuis `LR=0x820DC374` au tick
2511/thread 1, est un thunk PAL de 16 octets. Ses bytes exacts sont
`81830000816C009C7D6903A64E800420` : chargement de la vtable depuis `r3`,
chargement du slot `+0x9C`, `mtctr`, puis `bctr`. Le `bctr` précédent à
`0x820D32AC`, le terminal à `0x820D32BC` et le sibling suivant à
`0x820D32C0` ferment ses bornes. Son SHA-256 est
`da23e3ba38b71445cb1e786baee20d64c660f9db6f185a5984909c92625ae284`.

Après codegen strict, neutral atteint `max_ticks=2512` avec 2 375
notifications de présentation et aucune frontière non résolue. Une extension
inchangée atteint ensuite `max_ticks=3000`, 2 863 présentations, toujours sans
frontend, mission ou terminal. START n'a pas été injecté.

Le codegen compte 12 873 fonctions et 151 records configurés, avec zéro
diagnostic de frontière et zéro instruction unsupported. Deux générations
fraîches de l'atlas sont byte-identiques : SHA-256
`05a261a153c1b415fdee94aff65900946dc0e8074ee9d234b1d1bc1c60f0b71b`,
couverture `.text` 3 041 220/3 041 220, 801 vtables et 7 415 sites indirects.
L'atlas précédent est conservé sous
`/fastdata/lavaulta/tmp/ac6-demo-static-decomp-atlas-v1.pre1744.json`.

Validations : pytest 67 tests + 4 subtests, CTest codegen OFF 18/18 et ON
17/17. Les IB restent ceux qualifiés (`ef7ab6e4…d2b0` et
`d121c8d8…358d6`), mais aucune présentation ne devient une preuve de pixel ou
de frontend.

Le prochain checkpoint neutral est désormais temporel : prolonger la même
route au-delà de tick 3000 jusqu'au prochain trap réel ou à un état frontend
guest-owned. Aucun nom retail, START, pixel, audio ou résultat de mission n'est
promu.

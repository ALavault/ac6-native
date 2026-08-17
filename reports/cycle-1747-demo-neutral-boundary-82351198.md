# Cycle 1747 — frontière neutral `0x82351198` fermée

Cible exclusive : démo PAL `Default.xex`, SHA-256
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.
Aucune preuve retail n'est fusionnée.

Neutral étendu au-delà de 5000 ticks piège à tick 5049/thread 21 sur
`LR=0x8235392C -> 0x82351198`, après 4912 présentations et sans frontend.
Le callsite PAL `0x82353928` est un `bctrl` dans la fonction `.pdata`
`0x823538F8..0x8235393F`. Il charge la cible depuis le mot `+0x0C` d'une table
pointée par l'objet et passe cet objet dans `r3`.

La cible n'est pas une entrée `.pdata`. Elle est une sous-entrée exacte du
chunk Ghidra canonique `0x82351190..0x823511AF` :

```text
0x82351198  3863FFFC  addi r3,r3,-4
0x8235119C  480001CC  b    0x82351368
```

Les huit bytes portent le SHA-256
`edc2a88af607b72e14d53b5d8b68388e5b565ccc2e67839abde0a345f45cf68b`.
La destination `0x82351368` est une fonction `.pdata` déjà qualifiée. La forme
est donc un thunk d'ajustement de `r3` avec tail branch ; son type et son rôle
métier restent `unknown`.

Après ajout du seul record borné, le codegen strict produit 12 875 fonctions,
153 records configurés, zéro diagnostic de frontière et zéro instruction non
supportée. Deux atlas frais sont byte-identiques, SHA-256
`0947f8550beb70460bd722d65704fd11b50262f4d4e7220c1eb09d503d39c491`,
avec 3 041 220/3 041 220 bytes `.text` classés.

Le replay traverse le thunk et atteint, au même tick/thread, la prochaine
frontière : `xboxkrnl.exe:XMAReleaseContext`, ordinal 550, thunk
`0x823767A4`, depuis `LR=0x82356820`. Son rapport porte le SHA-256
`66f89741b42762326c301c3b7a22bfbc0b40e8a0c981d9c2891364adb44fb27c`
et sa trace `0b553e06ccd6cfbf72302d2ee3063bc8e44fcf4278b873ecd590d728abb222af`.

CTest passe codegen OFF 18/18 et ON 17/17, ainsi que 67 tests Python. Le
prochain checkpoint doit qualifier le pointeur de contexte et l'effet exact de
l'ordinal 550, puis n'implémenter que le tuple atteint sans inventer de
décodage XMA. START, pixel non noir, audio audible et mission restent ouverts.

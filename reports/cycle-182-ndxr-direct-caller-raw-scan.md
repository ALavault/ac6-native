# Cycle 182 — caller direct de l'entrée NDXR trouvé par balayage brut

Date : 2026-07-18 (Europe/Paris)

## Cible

- target ID : `ac6-xbox360-pal`
- module : `default.xex`
- SHA-256 : `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- projet Ghidra : `ace-combat-6`
- base : `0x82000000`

Passe headless et statique, en lecture seule, sans session humaine, VNC ou
Xenia.

## Pourquoi un balayage brut

`FindPpcBranchesTo.java` parcourt uniquement les instructions que Ghidra a déjà
définies. La zone autour de `0x82233298` contient des fragments `.pdata` très
courts et plusieurs mots non désassemblés ; un appel direct pouvait donc être
absent de la base de références. Le script ajouté
`FindPpcRawBranchesTo.java` décode les instructions PPC dans les blocs mémoire
initialisés et exécutables, sans modifier le programme.

## Appel trouvé

Le balayage brut produit :

```text
0x822341bc -> 0x82233298 raw=0x4bfff0dd
```

Le décodage headless borné autour de ce site donne :

```text
0x82234194  lwz  r3,0x1330(r31)
0x82234198  or   r5,r29,r29
0x8223419c  fmr  f1,f24
0x822341a0  lwz  r11,0(r3)
0x822341a4  lwz  r11,0x3c(r11)
0x822341a8  mtspr CTR,r11
0x822341ac  bctrl
0x822341b0  or   r5,r29,r29
0x822341b4  or   r3,r31,r31
0x822341b8  fmr  f1,f24
0x822341bc  bl   0x82233298
```

Le prologue du corps `.pdata` commençant à `0x82234040` établit `r31=r3` et
prépare `r29=r31+0x60` (`0x822340ac`). Le caller passe donc explicitement :

```text
r3 = r31
r5 = r31 + 0x60
f1 = f24
```

Le contrat déjà établi pour `0x82233298` relie ensuite `r5` à `r27`, puis lit
`r27+0x40`, soit le sous-objet `r31+0xa0`. Le même corps prépare ensuite
`r30=r29+0x40` (`0x822341e0`) avant la voie directe vers `sub_822131d0` à
`0x82234294`.

## Portée et confiance

- `confirmed` : l'existence du `bl` direct `0x822341bc -> 0x82233298` dans un
  bloc exécutable du XEX.
- `confirmed` : la préparation ABI `r3=r31`, `r5=r31+0x60`, `f1=f24` au site
  d'appel.
- `confirmed` : `0x82234040` est l'entrée `.pdata` précédente et
  `0x82234f18` l'entrée suivante ; le caller appartient donc au corps borné
  `0x82234040..0x82234f17`, malgré les fragments Ghidra courts.
- `confirmed` : la relation statique `r31+0x60 -> (r31+0xa0)` via le contrat
  du callee et la préparation `r30=r29+0x40`.
- `unknown` : type C++, unité des quatre mots, rôle des appels virtuels voisins
  (`+0x6c`, `+0x3c`, `+0x68`) et sémantique gameplay.
- `needs-dynamic-evidence` : vtable runtime du receiver partagé et identité
  métier du payload NDXR.

Cette preuve remplace uniquement l'affirmation précédente « aucun caller direct
trouvé ». Elle ne transforme pas le receiver ou le payload en avion, caméra,
position ou rendu.

## Preuves exécutées

```text
FindPpcRawBranchesTo.java 0x82233298 0x822131d0
InspectFunctionIsland.java 0x822338a8 0x822353b8
DumpRange.java 0x82234040 0x822342f0
DumpDataWords.java 0x820814b0 36
```

Le balayage brut a aussi retrouvé les appels directs connus vers
`0x822131d0`, dont `0x82233550` et `0x8223376c`; aucune écriture de projet ou de
sortie générée n'a été effectuée.


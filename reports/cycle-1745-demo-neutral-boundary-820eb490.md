# Cycle 1745 — frontière neutral `0x820EB490` fermée

Neutral atteint à tick 4251/thread 1 la cible indirecte `0x820EB490` depuis
`LR=0x820EA288`. La cible est le dernier des quatre thunks de 24 octets du
chunk PAL `0x820EB448..0x820EB4A7`. Elle charge l'objet global
`0x823CAB58`, puis son slot virtuel `+0xE8`, et termine par `bctr` à
`0x820EB4A4`. Le sibling `.pdata` suivant commence exactement à
`0x820EB4A8`.

L'atlas RTTI joint la vtable `0x82007D60`, slot 7, à cette cible. Ses bytes
exacts sont `3D60823D806BAB5881630000816B00E87D6903A64E800420`, SHA-256
`e88e5c7368a1e567db263db2851c38c7ac69ae2e0f2daa9809af5f8cc450851b`.
Le rôle sémantique du slot reste inconnu.

Après codegen strict, neutral atteint `max_ticks=4252` avec 4 115
présentations et aucune frontière non résolue. Une extension inchangée piège à
tick 4254 sur l'import qualifié `xam.xex:XMsgStartIORequest` ordinal 503,
`LR=0x821A55A0`. Les registres observés sont `r3=0xFB`, `r4=0x000B0006`,
`r5=0`, `r6=0x7F0409B8`, `r7=24`. Xenia/ReXGlue génériques identifient cette
forme comme le message XGI de contexte utilisateur, mais aucun effet n'est
encore implémenté ou promu dans la recompilation.

Le codegen compte 12 874 fonctions et 152 records configurés, zéro diagnostic
de frontière et zéro instruction unsupported. Deux générations fraîches de
l'atlas sont byte-identiques : SHA-256
`4e39111c83b9d124e02577fa707eb0815b2bfe2bc58ea4315f9691ae589230a2`,
couverture `.text` 3 041 220/3 041 220, 801 vtables et 7 415 sites indirects.
Pytest passe 67 tests + 4 subtests, CTest codegen OFF 18/18 et ON 17/17.

Le prochain checkpoint doit qualifier le buffer PAL de 24 octets et le retour
exact de l'ordinal 503, puis implémenter seulement le tuple atteint de façon
transactionnelle et fail-closed. START, frontend, pixel non noir, audio et
mission restent non qualifiés.

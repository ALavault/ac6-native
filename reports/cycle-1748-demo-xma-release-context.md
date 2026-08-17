# Cycle 1748 — trois contextes XMA libérés

Cible exclusive : démo PAL `Default.xex`, SHA-256
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.
Aucune preuve retail n'est fusionnée.

Neutral atteint à tick 5049/thread 21 trois appels consécutifs à
`xboxkrnl.exe:XMAReleaseContext`, ordinal 550, thunk `0x823767A4`, depuis le
callsite PAL `0x8235681C` (`LR=0x82356820`). Les pointeurs exacts sont
`0x2E800000`, `0x2E800040` et `0x2E800080`, soit les trois contextes de 64
octets alloués par le corridor `XMACreateContext` qualifié au tick 1048.
Les trois appels portent aussi `r4=1` et `r5=r6=r7=0`.

Le checkout ReXGlue générique épinglé au commit
`cb58065c793429aa92895d778af58d12e9d26d8f` invalide le bit d'allocation et
met les 64 octets à zéro lors d'une libération. Cette règle est classée
`xenia-generic`; la preuve PAL reste le tuple dynamique et la provenance des
trois allocations.

Le runtime adapte uniquement ces trois slots dans l'expérience XMA existante.
Il exige LR, registres, alignement, plage possédée/mappée et état actif. Une
libération remet les 64 octets à zéro, invalide le slot et retourne `r3=0`.
Les pointeurs hors tableau, non alignés, futurs ou déjà libérés sont rejetés
avant mutation. Aucun paquet n'est décodé et aucun audio n'est synthétisé.

Le replay final traverse les trois appels (`count=3`) et atteint la prochaine
frontière au même tick/thread : `LR=0x8236E588 -> 0x8236E550`. Le rapport porte
le SHA-256 `3d13810b02909ac56cf03f63f343177062513d86a553d5436daa2de6e464666a`,
la trace `0b553e06ccd6cfbf72302d2ee3063bc8e44fcf4278b873ecd590d728abb222af`
et le binaire `b6b5e71c1fcce2d7a5642dbd6dd35883d74fa9ff722eb844951865e34e53e1bf`.

CTest passe OFF 18/18 et ON 17/17. Un premier lancement simultané des deux
matrices a fait collision sur le fichier temporaire fixe du test frontier ;
la matrice ON rejouée seule passe 17/17. Cette collision n'affecte ni le guest
ni les artefacts de replay.

Le prochain checkpoint est la frontière `0x8236E550`, sans attribuer de rôle
avant qualification des bytes PAL. Frontend, pixels non noirs, audio audible,
START causal et mission restent ouverts.

# AC6 `ac6-demo-native` — owned IPC cycle 1767

Verdict : **IPC-CONTRACT-GO / RUNTIME-NO-GO**, `supported=false`.

Le commit `95d51538` expose la frontière plateforme/replay par un processus
`ac6-demo-native` lancé uniquement depuis une configuration serveur. Le serveur
possède le processus et une socketpair AF_UNIX privée ; aucune commande, socket,
HID, horloge, réseau ou adresse mémoire n'est fournie par l'appelant. Les trames
sont bornées (préfixe longueur big-endian de 4 octets, 64 KiB), JSON canonique,
token hexadécimal et XInput typé.

La séquence C++ socketpair `start → step → observe → stop` est exacte (`tick=1`,
`PRESENT=0`) et les trames surdimensionnées ferment la session. Le MCP v2 peut
sélectionner `demo-native` uniquement si le binaire a été configuré au démarrage;
les observations exposent alors les faits tick/PRESENT mais tous les domaines
guest/gameplay restent explicitement `unavailable`. Le replay garde le même
chemin de transport et ne propose aucun savestate ni accès mémoire arbitraire.

```text
cmake --build reconstruction/ac6-demo-native/build -j16       PASS
SDL_AUDIODRIVER=dummy xvfb-run -a ctest                      6/6 PASS
MCP transport tests                                             12 PASS, 1 skip
native binary start/step/observe/close                         PASS
complexity audit                                                 PASS (20 fichiers)
cmake --install ... ; test ! -e bin/bin                       PASS
```

Cette preuve ferme seulement l'exposition IPC de domaine 2. Elle ne qualifie
ni le scheduler guest, ni XAM/Xenos, ni frontend, mission, objectif, terminal,
readback ou support produit.

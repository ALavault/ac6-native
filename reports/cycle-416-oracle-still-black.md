# Cycle 416 — GPU présent, oracle toujours sans image

## 1. Le GPU n'est pas le manque

Contrairement à ce que supposait le cycle 415, l'accélération est disponible :

```
deviceName = NVIDIA RTX PRO 4000 Blackwell   driverName = NVIDIA
deviceName = llvmpipe (LLVM 21.1.8)          driverName = llvmpipe
```

`/dev/dri/renderD128` existe. Le runtime natif rend d'ailleurs à 60 Hz sur Xvfb,
ce qui le confirmait déjà : Vulkan n'a pas besoin de X pour *rendre*, seulement
pour *présenter*.

Ma conclusion du cycle 415 — « il manque un affichage adossé au GPU » — était
donc **prématurée**. Le GPU était là ; je ne l'avais pas vérifié avant de
conclure.

## 2. Nouvelle tentative, toujours sans image

Bureau virtuel Wine, backend forcé (`--gpu=vulkan`), déport NVIDIA
(`__NV_PRIME_RENDER_OFFLOAD`, `__GLX_VENDOR_LIBRARY_NAME`), deux captures à 90 s
et 130 s : **267 octets chacune**, soit vide.

Et cette fois la sortie standard du processus est **entièrement muette** (0
ligne), là où la tentative du cycle 415 affichait au moins trois fenêtres sans
erreur.

## 3. Piège évité de justesse

`xenia.log` contient bien `CreateWindowExW failed` — mais daté du fil `00000128`,
celui de la tentative **du cycle 414**. Ce journal n'est pas réécrit à chaque
lancement. Le lire sans regarder l'identifiant de fil aurait conduit à rapporter
une erreur qui n'appartient pas à cette exécution.

C'est exactement le type d'erreur commis quatre fois dans ce dossier. Signalé
ici pour la reprise : **vérifier le fil et l'horodatage de `xenia.log` avant de
lui attribuer quoi que ce soit.**

## 4. État

Aucune comparaison à l'oracle n'a été obtenue en trois cycles d'essais. Ce qui
est acquis : le montage Wine à bureau virtuel lance bien Xenia (cycle 415), le
GPU est disponible (ici), et la cause du noir reste **inconnue** — ni le
gestionnaire de fenêtres, ni l'accélération, contrairement aux deux hypothèses
successives que j'ai avancées.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

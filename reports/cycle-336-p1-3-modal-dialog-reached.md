# Cycle 336 — P1.3 : dialogue modal atteint, navigation vivante, confirmation inerte

## 0. État

**P1.3 n'est pas franchie.** Le sélecteur de campagne n'est pas atteint.
La traversée s'arrête sur un **dialogue modal OUI/NON**, où la navigation
fonctionne et la validation non.

## 1. Progression obtenue

Écran-titre -> (Start) -> ... -> **dialogue modal OUI/NON**, deux boutons côte
à côte. Reçus par l'invité, mesurés à la frontière `XamInputGetState` :

```
buttons=0x0010  START        1x
buttons=0x1000  A            5x
buttons=0x0004  DPAD_LEFT    1x
FATAL                        0
```

`DPAD_LEFT` déplace réellement la sélection : la capture avant montre **NON**
surligné, la capture après montre **OUI**. Changement d'image de 7,700 sur ce
seul appui. **La navigation du dialogue répond à l'entrée.**

## 2. Un piège de plus, levé

La première traversée s'est **arrêtée net** au premier appui sur A :

```
[FATAL] Unresolved branch from 0x8237D008 to 0x8237CE18
```

Même cascade qu'aux cycles 332-333 : l'entrée fait progresser l'invité dans du
code jamais atteint, qui expose la frontière suivante. Déclarée en
`[functions]`, régénérée, reconstruite — la traversée passe ensuite sans aucun
`FATAL`.

À noter pour ne pas mal lire les mesures : après ce plantage, toutes les
différences d'image valaient `0,000`. Ce n'était **pas** un écran statique mais
un **processus mort**, le serveur X continuant d'afficher la dernière image.
Un `0,000` persistant doit toujours être confronté à la vivacité du processus.

## 3. Ce qui bloque

**Cinq appuis sur A ne valident pas le dialogue.** Il reste affiché, sélection
sur OUI, l'invité tourne (58,98 im/s, 227 612 dessins hôte, aucun `FATAL`).

L'invité *reçoit* bien `0x1000` — c'est mesuré à la frontière. Donc soit :

1. ce dialogue ne se valide pas par A mais par un autre bouton (B = `Shift`,
   ou Start) — hypothèse la moins coûteuse, non testée ;
2. le maintien de 0,6 s est lu comme un appui **continu** et non comme une
   transition, et le dialogue attend un front montant que la cadence
   d'interrogation ne lui donne pas ;
3. la validation dépend d'un état non encore fonctionnel (périphérique de
   sauvegarde, profil utilisateur), auquel cas le dialogue est un prompt de
   stockage et sa réponse mène à un chemin non implémenté.

Les trois se départagent en une exécution : essayer B et Start, et faire varier
la durée de maintien.

## 4. Erreur de méthode corrigée en cours de route

La première traversée envoyait `Down`/`Up` à un dialogue dont les deux boutons
sont **côte à côte**. Aucun receipt de D-pad n'est apparu, et j'ai d'abord lu
cela comme « l'invité ne reçoit plus rien ». C'était mon test qui envoyait le
mauvais axe. Avec `Left`, le receipt `0x0004` apparaît et la sélection bouge.

Regarder la disposition à l'écran avant de choisir l'axe de navigation.

## 5. Ce qui reste

- P1.3 — sélecteur de campagne : **non atteint**.
- P2 à P7 : non faits. La première mission ne se joue pas.

`recompiler-generated` n'est pas `verified`.

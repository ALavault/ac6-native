# AC6 — provenance du sous-objet et de la vtable NDXR

Date : 2026-07-17 (Europe/Paris)

## Cible et méthode

Cible : `default.xex` Xbox 360 PAL, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

Cette passe reste headless, statique et en lecture seule. Elle complète les
qualifications des cycles 140 à 142 sans modifier le projet Ghidra, les
exports ou le runtime.

Commandes principales :

```bash
./.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -readOnly -noanalysis -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript DumpRange.java 0x8212a1e0 0x8212a360

./.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -readOnly -noanalysis -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript DumpRange.java 0x820fabf0 0x820fad50

./.tools/ghidra_12.1.2_PUBLIC/support/analyzeHeadless \
  workspaces/ace-combat-6/ghidra-projects ace-combat-6 \
  -readOnly -noanalysis -process default.xex \
  -scriptPath workspaces/ace-combat-6/scripts \
  -postScript DumpDataWords.java 0x8205d680 100
```

## Sous-objet construit

Dans le chemin d'allocation autour de `0x8212a2a8` :

```text
0x8212a29c  addi r3,r31,0x14
0x8212a2a0  subi r11,r11,0x2940
0x8212a2a4  stw  r11,0x0(r31)       ; vtable de l'objet extérieur
0x8212a2a8  bl   0x820f9dc8        ; constructeur du sous-objet
```

L'initialiseur `0x820f9dc8` reçoit donc `r3 = outer+0x14` et écrit
`0x8205c980` à l'offset zéro de ce sous-objet. Cela explique pourquoi il faut
séparer :

- la vtable de l'objet extérieur, observée ici à `0x8205d6c0` ; et
- la vtable du sous-objet, observée ici à `0x8205c980`.

Il ne faut plus appeler `0x8205c980` « la vtable de tout l'objet » sans
qualifier le sous-objet `+0x14`.

## Méthodes de la même table

Le dump de `0x8205c980` montre :

```text
vtable + 0x10c = 0x820fbc28
vtable + 0x110 = 0x820fa9c0
vtable + 0x05c = 0x82101be0
vtable + 0x13c = 0x821002f0
```

Les deux premières adresses sont précisément les deux fonctions dont les
appels vers `0x82105ba8` ont été retrouvés aux cycles 139/140. Cela donne une
preuve `cross-match` forte que ces fonctions appartiennent au même sous-objet
polymorphe que les slots candidats. Cela ne démontre pas encore que chaque
instance exécutée par le worker conserve ce vtable sans dérivation ou
remplacement ultérieur.

## Champ `+0x28` du sous-objet

Dans `0x820fa9c0`, le sous-objet reçoit plusieurs valeurs de ressources ; les
écritures observées incluent :

```text
0x820fac34  stw r11,0x28(r31)
0x820fad18  stw r26,0x28(r31)   ; remise à zéro sous condition
```

Le worker lit ensuite `context+0x28` comme base de parcours, puis charge un mot
à l'offset calculé dans cette base. La provenance de la valeur précise et son
contenu au moment des trois appels restent dynamiques ; il est seulement sûr
de qualifier `+0x28` comme champ consommé par le worker et initialisé par la
famille de méthodes ci-dessus.

## Contradiction `r4` toujours ouverte

Le sous-objet/vtable est maintenant mieux corrélé, mais l'instruction du worker
reste :

```text
rlwinm r4,r31,0x10,0x17,0x1f
```

Elle produit un champ de 9 bits, alors que `0x82101be0` lit `r4+0x1c` comme une
adresse. Les hypothèses encore possibles incluent une autre vtable effective,
un encodage/alias runtime ou une compréhension incomplète du chemin de
registre. Aucune ne doit être promue sans preuve. Le résultat transmis en `r5`
reste donc « sortie du dispatch indirect », et non encore « mot de
`record+offset+0x08` ».

## Niveau de confiance

`confirmed` :

- appel de `0x820f9dc8` sur `outer+0x14` dans le chemin observé ;
- écriture de `0x8205c980` comme vtable du sous-objet ;
- présence des deux méthodes appelantes dans cette table ;
- lecture du champ `context+0x28` par le worker et écritures voisines dans la
  famille de méthodes.

`cross-match` :

- appartenance des appelants et des slots à la même famille de sous-objet.

`unknown` :

- vtable réellement utilisé par chaque instance au moment du worker ;
- encodage de `r4` et identité de la cible effective du slot `+0x5c` ;
- nature de la table pointée par `+0x28` et relation avec les ressources NDXR.

## Validation native

Après cette passe documentaire, la suite AC6 a été rejouée :

```bash
git diff --check
ctest --test-dir .build/ace-combat-6/native --output-on-failure
```

Résultat : **41/41 tests passés**, aucun échec, durée 16,42 s. Aucun code natif
ou export généré n'a été modifié.

## Suite

Rechercher les remplacements de vtable après `0x820f9dc8`, puis suivre la
création et le remplissage de la table consommée par `context+0x28`. Une capture
d'état runtime ne sera demandée que si ces deux pistes statiques ne départagent
pas les hypothèses. Aucune action humaine n'est requise maintenant.

# AC6 — appel global du destructeur et helpers de nettoyage (cycle 160)

Date : 2026-07-17 (Europe/Paris)

## Cible et méthode

Cible : `default.xex` Xbox 360 PAL, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

Passe headless en lecture seule avec `DumpRange.java`, `InspectFunctionIsland`
et `DumpDataWords.java`. Les plages inspectées couvrent l'appel externe
`0x823d4ed8`, le helper `0x821065e0` et les wrappers proches de
`0x82335f38`. Aucun projet, XEX ou runtime n'a été écrit.

## Résultats

### Destruction d'une instance globale

À `0x823d4ed0..0x823d4ed8` :

```text
0x823d4ed0  lis  r11,-0x7d6c
0x823d4ed4  subi r3,r11,0x4570
0x823d4ed8  b    0x820fa6f0
```

L'adresse construite est `0x8293ba90`, dans une zone de données actuellement
initialisée à zéro dans le fichier analysé. Le bloc effectue un tail-branch vers
le même chemin de destruction que le wrapper local. Cela qualifie un second
point d'appel global, mais ne prouve pas à lui seul quand l'instance reçoit son
vtable à l'exécution.

### Helper `0x821065e0`

Le helper reçoit un sous-objet, lit `subobject+0xd0`, puis, pour trois entrées
à partir de `subobject+0x200`, invoque le slot `vtable+0x8` avant de passer le
pointeur au service `0x82222f20`. Il remet ensuite chaque entrée à zéro.

Le destructeur NDXR appelle ce helper sur des sous-régions à partir de
`receiver+0x62e0` et les avance par `0x2b0`. Cette preuve établit une chaîne de
nettoyage de sous-objets, mais ces sous-régions ne sont pas le pointeur publié
par `context+0x30`.

### Wrapper `0x82335f38`

`0x82335f38` prépare un contexte global et tail-branche vers `0x82340640`.
Le dump ne montre pas de chargement de `receiver+0x30` ni de passage direct de
ce champ à ce wrapper. Il ne doit donc pas être déclaré comme le libérateur de
la zone publiée.

## Décision de preuve

`confirmed` :

- `0x823d4ed8` est un appel global supplémentaire vers `0x820fa6f0` ;
- `0x821065e0` nettoie des sous-objets par destructeur virtuel et appelle
  `0x82222f20` pour leur pointeur ;
- la chaîne de nettoyage NDXR couvre les tables et sous-objets, puis efface
  `+0x30`.

`unknown` :

- si `0x82222f20` libère le buffer publié par `+0x30` dans un chemin non montré ;
- si ce buffer est une vue de sous-objet ou un résultat temporaire sans
  ownership indépendant ;
- le consommateur de la zone après `0x82106344`.

Aucune action humaine n'est nécessaire. La prochaine frontière statique est de
suivre les appels à `0x82222f20` avec leurs registres d'entrée et de comparer
les offsets passés avec `+0x30`, `+0xd0` et les bases `+0x62e0/+0x6500`.

## Validation documentaire

- `DumpRange.java` sur `0x823d4c80..0x823d4f60` : PASS ;
- `DumpRange.java` sur `0x82106580..0x82106680` : PASS ;
- `DumpRange.java` sur `0x82335ed0..0x82336090` : PASS ;
- `DumpDataWords.java` sur `0x8293ba90` (16 mots) : PASS ;
- aucune écriture Ghidra/XEX/générée/runtime : PASS.

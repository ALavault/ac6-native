# AC6 — frontière du receiver runtime avant le slot `+0x140` (cycle 175)

Date : 2026-07-18 (Europe/Paris)

## Cible et provenance

Cible canonique AC6 Xbox 360 PAL : `default.xex`, target ID
`ac6-xbox360-pal`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`, base
`0x82000000`, projet Ghidra `ace-combat-6`.

La preuve est exclusivement headless : `DumpDataWords.java`,
`ReferencesTo.java` et le désassemblage XenonRecomp. Aucun projet Ghidra,
sortie générée ou source natif n'a été modifié.

## Provenance de la table d'objets

Dans `sub_822131d0`, le registre `r25` reçoit le pointeur lu à l'adresse
globale `0x826e4eb4`. Le dump statique de cette cellule contient
`0x829e6720`; les références headless montrent de nombreux consommateurs de
ce pointeur comme table runtime.

Le site `0x82213558` forme ensuite l'offset immédiat `0x36084` et exécute :

```text
r3 = *(r25 + 0x36084)
vtable = *(r3 + 0)
target = *(vtable + 0x140)
```

Le writer d'initialisation `0x8226a980` confirme la construction de cette
cellule : il écrit à `table+0x36084` la valeur `table+0x3607c` (les deux
adresses étant calculées à partir du même pointeur global). Le receiver du
dispatch est donc une adresse de structure dérivée de la table, pas une
adresse de fonction ou une vtable statique directement stockée dans le XEX.

## Limite statique

La mémoire initiale de `0x829e6720` et de `0x82a1c79c` est nulle dans l'image
statique. Le premier mot du receiver `table+0x3607c`, qui doit fournir le
vtable au moment du dispatch, n'est donc pas résolu par l'analyse statique.

Conséquence :

```text
0x8205c9a4 + 0x140 -> 0x82102e70 : cross-match confirmé par la table candidate
receiver runtime réellement utilisé   : needs-dynamic-evidence
```

Il est impossible de promouvoir sans trace runtime la relation
`*(table+0x36084)` → address-point `0x8205c9a4`. La limite est circonscrite à
l'identité du receiver/vtable ; elle ne remet pas en cause le contrat ABI,
les deux records ni les sorties déjà établis aux cycles 173–174.

## Classement opérationnel

- `KEEP` : conserver le routage headless et les contrats par registres ;
- `KEEP_WITH_CLARIFICATION` : conserver le slot `+0x140` comme candidat
  compatible, sans l'appeler méthode NDXR confirmée ;
- `needs-dynamic-evidence` : capturer ultérieurement le premier mot de
  `table+0x3607c` au moment du dispatch, ou un équivalent par trace mémoire ;
- aucune action humaine maintenant : cette preuve manquante est localisée et
  peut attendre une session runtime planifiée.

La prochaine passe statique utile est de suivre les écritures vers
`table+0x3607c` et les fonctions qui initialisent ce receiver, sans modifier
les sorties générées.

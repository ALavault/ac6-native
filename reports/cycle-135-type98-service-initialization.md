# AC6 — initialisation statique du service de ressources lié au chemin type `0x98`

Date : 2026-07-17 (Europe/Paris)

## Cible et périmètre

La cible est le `default.xex` Xbox 360 PAL de la version qualifiée, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`.

Cette passe est une lecture headless du projet Ghidra corrigé. Elle ne lance ni
Xenia, ni Wine, ni GUI, et n'applique aucun patch au projet Ghidra. Le but est
de fermer une partie de l'initialisation du service utilisé par le chargeur
d'entrée 9, sans confondre un contrat d'allocateur avec un composant de jeu.

## Résultat

Le stockage global lu à l'adresse `0x826a0728` est maintenant borné comme un
objet de service initialisé à l'exécution. Le corps compilateur-split autour de
`0x82093d38` montre :

1. un appel indirect par le slot virtuel `+0x24` du service, avec l'argument
   d'origine et le second argument nul ;
2. une préparation de stockage avec les constantes `0x120` et `0x10` via
   `0x82228da8` ;
3. un appel indirect par le slot virtuel `+0x10` avec le littéral `0x9a` ;
4. la remise à zéro des champs temporaires `service+0x04` et `service+0x08`.

Cela établit une phase d'initialisation/enregistrement de ressource. Cela ne
prouve pas encore que `0x9a` est le type `0x98`, ni que le service est un
gestionnaire d'avions, de scène ou de rendu.

## Preuves d'assembleur

Dans `reports/82093c80-service-use.log`, l'adresse globale est formée par
`lis r31,-0x7d96` puis `lwz r3,0x728(r31)`, donc le pointeur de service réside à
`0x826a0728` dans la convention d'adresses de cette image.

Le bloc `0x82093d38..0x82093e14` effectue ensuite :

```text
82093d54  lwz  r3,0x728(r31)
82093d58  lwz  r11,0x0(r3)
82093d5c  lwz  r11,0x24(r11)
82093d60  mtspr CTR,r11
82093d64  bctrl
...
82093d74  li   r5,0x10
82093d7c  bl   0x82226498
82093d80  lwz  r11,0x728(r31)
82093d8c  stw  r3,0x50(r1)
82093d90  stw  r10,0x4(r11)
82093d94  stw  r28,0x8(r11)
...
82093dd0  li   r4,0x120
82093dd4  or   r3,r11,r11
82093dd8  bl   0x82228da8
82093dde  li   r7,0x9a
82093df0  lwz  r11,0x0(r3)
82093df4  lwz  r11,0x10(r11)
82093df8  mtspr CTR,r11
82093dfc  bctrl
82093e04  or   r3,r30,r30
82093e08  stw  r28,0x4(r11)
82093e0c  stw  r28,0x8(r11)
```

Les trois appels directs à `0x82093d38` proviennent de `0x82093c70`,
`0x82093c9c` et `0x82093cd0`. La fonction n'est pas représentée comme un corps
continu par la table de fonctions Ghidra, car elle partage les entrées de
sauvegarde/restauration Xenon ; le `DumpRange` reste donc la source de vérité
pour ce fragment.

## Rôle des helpers réutilisés

Les décompilations headless donnent les contrats suivants :

- `0x822383d0(view,index)` vérifie `index < view->count`, lit l'offset de la
  table et retourne `view->base + offset`, sinon zéro ;
- `0x82238408(view,index)` vérifie la borne et retourne l'entrée de la table
  auxiliaire ;
- `0x822283e8(object)` initialise les champs `+0x30..+0x7c` avec les constantes
  globales de configuration et appelle `0x82227ab0` ;
- `0x82228da8(service,0x120,0x10)` augmente le curseur pointé par
  `service+0x04` de `0x120`, puis l'aligne sur `0x10` lorsque ce pointeur est
  disponible ;
- `0x823864f4` et les entrées voisines sont les helpers de sauvegarde des
  registres non volatils Xenon. Ils ne doivent pas recevoir de nom runtime.

Le corps de `0x82226498` est lui aussi découpé par un prologue partagé ; cette
tranche documente uniquement son appel depuis le service avec les paramètres
observés et ne le renomme pas en allocateur confirmé.

## Relation avec le chemin type `0x98`

Les rapports `ENTRY9_CHILD1_CONSUMER_REPORT.md` et
`ENTRY9_X360_UNIT_MANAGER_REPORT.md` établissent séparément que le chargeur
d'entrée 9 envoie les éléments MDLP au service global avec les slots `+0x18`,
`+0x1c` et `+0x10`, en passant le type littéral `0x98`, et stocke un résultat à
l'objet construit `+0x15c`.

Le nouveau fragment prouve l'initialisation de la même famille de service
globale (`0x826a0728`) et montre que son slot `+0x10` accepte aussi un
discriminateur `0x9a` pendant cette phase. La relation exacte entre les deux
littéraux reste ouverte : il est interdit de les fusionner sans trace runtime,
table de types ou cible vtable correspondante.

## Frontière de confiance

`confirmed` :

- global de service à `0x826a0728` dans cette image ;
- dispatch indirect via les slots `+0x24` et `+0x10` ;
- constantes et ordre des écritures `+0x04/+0x08` ;
- appel d'alignement `0x120`/`0x10` ;
- littéral `0x9a` au site `0x82093de0`.

`unknown` :

- classe concrète et adresse runtime de la vtable derrière `0x826a0728` ;
- sémantique métier de `0x9a` ;
- identité des objets retournés par les slots `+0x24`, `+0x10`, `+0x18` et
  `+0x1c` ;
- jonction vers une soumission de dessin, une traversée de scène ou un avion
  actif.

## Prochaine étape statique

Suivre la provenance de la vtable runtime de `0x826a0728` dans les initialises
XEX et comparer les appels `+0x10`/`+0x18`/`+0x1c` sur un même contexte. La
prochaine étape ne doit pas transformer `0x9a` en alias de `0x98` sans preuve.
Une trace Xenia ou une session humaine pourrait aider plus tard à attribuer la
classe concrète, mais elle n'est pas nécessaire pour continuer l'analyse
statique actuelle.

## Validation

- Ghidra `analyzeHeadless` en lecture seule avec `DumpRange.java`,
  `ListFunctionsRange.java`, `FindDirectCallsTo.java` et `DecompileAt.java`.
- CTest AC6 : 41/41 attendu sur le dernier run validé.
- Aucune session GUI, Xenia, Wine, VNC ou intervention humaine.

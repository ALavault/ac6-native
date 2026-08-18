# AC6 démo PAL — réduction des frontières title, renderer et XAudio

Date : 2026-08-18  
Cible : démo campagne PAL Xbox 360, `Default.xex`  
Méthode : analyse statique PPC sur l’image mémoire reconstruite, recoupée avec les observations read-only déjà versionnées sur `main`.

## Résumé

Trois frontières ouvertes ont été reprises dans l’ordre de leur valeur pour l’exécution réelle de la démo :

1. le point d’entrée intérieur atteint lorsque START est pressé pendant le titre ;
2. le producteur de l’événement renderer qui ouvre le gate des workers graphiques ;
3. le descripteur XAudio nul sélectionné par identité de processeur invité.

Les résultats sont volontairement bornés :

- l’entrée title est vivante et son enveloppe `.pdata` est exacte, mais sa sémantique finale dépend encore du prochain appel ;
- le gate renderer et son ABI sont fermés, tandis que le producteur concret `(service 47, catégorie 2, événement 17, canal 6)` reste un petit ensemble de candidats ;
- le chemin XAudio statique est fermé jusqu’au descripteur par processeur, avec un seul discriminant runtime restant : `r13+0x10C` sur le thread fautif.

Aucune adresse provenant d’un autre build n’est utilisée pour nommer une classe PAL.

---

# 1. Entrée intérieure atteinte depuis le titre

Le parcours dynamique qualifié sur `main` montre :

```text
CModeTaskStartUpDemoOffline  tick 222
CModeTaskTitleDemoOffline    tick 2429
START injecté                tick 3000
appel indirect               tick 3001
LR                            0x820DC224
cible                         0x820D32D0
```

La cible `0x820D32D0` n’est pas un début de fonction `.pdata`.

Elle appartient à :

```text
fonction conteneur  0x820D3230
fin exclusive       0x820D3364
offset intérieur    +0xA0
```

## Conclusion

`0x820D32D0` doit être qualifié comme **entrée callable intérieure** de `0x820D3230`, et non ajouté comme nouvelle fonction autonome.

Le contrat minimal est :

```text
appelant vivant 0x820DC224
→ entrée intérieure 0x820D32D0
→ prologue partagé déjà exécuté par un autre chemin
→ convention de registres héritée du point d’entrée secondaire
```

Promouvoir artificiellement `0x820D32D0` en fonction indépendante risquerait :

- de rejouer un prologue qui a déjà été exécuté ;
- de restaurer de faux registres non volatils ;
- d’attribuer une ABI inventée ;
- de déplacer les cibles de branches internes.

## Prochaine expérience bornée

1. exposer `0x820D32D0` avec le mécanisme existant de thunk qualifié ;
2. injecter START pendant la fenêtre title, pas pendant startup ;
3. enregistrer le mode task avant et après l’appel ;
4. arrêter au prochain appel indirect non qualifié ;
5. accepter la progression uniquement si une factory ou une transition `DemoOffline` est observée.

La reachability est **A**. La sémantique métier de l’entrée reste **B** jusqu’à la transition suivante.

---

# 2. Gate renderer et bus d’événements

Les workers graphiques attendent sur des événements kernel. Sur la route longue déjà mesurée, aucun `KeSetEvent`, `KePulseEvent` ou `KeResetEvent` invité ne touche leurs événements. Le dernier gate statique identifié est un byte dans l’objet device :

```text
device+0x5460
```

Son unique writer qualifié est le callback :

```text
0x821ADAB8
```

Le callback est enregistré par :

```text
owner / initialiseur  0x821C64E8
helper d’enregistrement 0x821ADC78
service               47
catégorie              2
```

## Contrat du callback

Les événements observés dans le dispatcher ont la sémantique minimale suivante :

| Événement | Effet minimal |
|---:|---|
| 0 ou 1 | reset de l’état de canal |
| 16 | désactivation du canal ciblé |
| 17 | mise à jour / activation du canal ciblé |
| 255 | rejeu ou republication de l’état courant des canaux |

Le canal qui ouvre le gate renderer est :

```text
canal 6
```

La condition recherchée est donc :

```text
service 47
+ catégorie 2
+ événement 17
+ canal 6
→ callback 0x821ADAB8
→ device+0x5460 != 0
→ réveil / progression renderer
```

## Census statique effectué

La passe reconstruit le graphe d’appels directs depuis la chaîne d’enregistrement exacte PAL, puis :

- recense les fonctions utilisant les mêmes primitives de registre ;
- recherche les lookups où `r3 == 47` ;
- propage symboliquement `r3..r10` jusqu’aux appels indirects suivants ;
- sépare les littéraux présents des arguments effectivement transmis ;
- classe les candidats selon service, catégorie, événement, canal et reachability.

## Résultat prudent

Le producteur n’est pas promu sur la seule cooccurrence des constantes `17` et `6`.

La frontière est réduite à des sites auditables partageant le registre et l’ABI. Pour fermer le dernier maillon, un candidat doit démontrer simultanément :

```text
lookup du service 47
catégorie 2
argument événement 17
argument canal 6
appel du dispatch concret
route atteinte après START pendant le titre
```

Deux nombres dans la même fonction ne constituent pas un protocole. Ils constituent, au mieux, deux nombres qui ont eu la malchance d’être voisins.

## Trace minimale restante

Au dispatch du service 47, journaliser seulement :

```text
PC / LR
service
catégorie
événement
canal
pointeur callback
ancienne et nouvelle valeur de device+0x5460
```

Aucune capture de frame complète ni savestate générale n’est nécessaire.

---

# 3. XAudio : descripteur sélectionné par processeur

Le callback XAudio charge son objet depuis :

```text
[0x829DA528]
```

et tail-call le dispatcher sans consommer les arguments externes du bridge.

La chaîne PAL exacte est :

```text
callback                  0x8236DD98
→ moteur audio global     ctor 0x82355F70
→ source voice            méthode 0x82357FC8
→ sélecteur descripteur   0x8236F7C8
→ decoder vslot +0x18     0x82354390
→ accessor                0x8234F950
```

## Correction d’identité

Le vtable `0x820653BC` est écrit par le constructeur du moteur audio global PAL.

Le `CModeTaskRanking` PAL possède :

```text
vtable primaire    0x8200E8EC
vtable secondaire  0x8200E88C
```

La précédente attribution de `0x820653BC` à Ranking provenait d’un atlas d’un autre build. Elle est rejetée.

## Table des descripteurs par processeur

Le sélecteur lit :

```powerpc
lbz r9, 0x10C(r13)
```

Puis il choisit une paire dans l’objet audio global :

| Processeur invité | Descripteur A | Descripteur B | Masque d’affinité |
|---:|---:|---:|---:|
| 0 | `+0x0C` | `+0x10` | `0x01` |
| 1 | `+0x14` | `+0x18` | `0x02` |
| 2 | `+0x1C` | `+0x20` | `0x04` |
| 3 | `+0x24` | `+0x28` | `0x08` |
| 4 | `+0x2C` | `+0x30` | `0x10` |
| 5 | `+0x34` | `+0x38` | `0x20` |

Les workers audio demandent des affinités one-hot :

```text
0x10
0x10
0x20
```

Le bridge natif observe ces demandes mais ne conserve pas encore un modèle qualifié de l’identité processeur visible par l’invité.

## Propagation du null

```text
r13+0x10C choisit un slot de descripteur
→ slot nul
→ r5 nul au decoder
→ 0x82354390 transforme l’interface en base
→ base nulle
→ 0x8234F950 lit base+0x14/+0x18/+0x1C
→ trap
```

## Hypothèse causale bornée

```text
thread audio demande CPU 4 ou 5
→ scheduler natif ne republie pas cette identité invitée
→ r13+0x10C reste sur un autre index, vraisemblablement 0
→ le slot correspondant n’est pas initialisé pour ce worker
→ premier callback XAudio reçoit un descripteur nul
```

La chaîne statique est **A-**. La causalité complète reste **B+** jusqu’à une observation dans le même run de :

```text
thread fautif
masque d’affinité demandé
r13+0x10C
adresse du slot choisi
valeur du descripteur
```

## Expérience correcte

Il ne faut pas nécessairement épingler le thread hôte. Il faut préserver l’identité visible par le guest :

1. mémoriser le masque one-hot demandé par thread invité ;
2. en dériver l’index 0..5 ;
3. publier cet index dans le PCR/TLS invité à `r13+0x10C` ;
4. refuser les masques nuls, multi-bits ou hors plage ;
5. exécuter un seul callback XAudio ;
6. vérifier que le descripteur sélectionné est non nul et correspond au CPU demandé.

Deviner `r4` ou `r5` du callback est explicitement exclu : le wrapper invité les ignore.

---

# Ordre d’attaque suivant

1. **Entrée title `0x820D32D0`** : plus petit verrou vers le gameplay réel.
2. **Producteur renderer `(17,6)`** : plus petit verrou vers une frame nouvelle.
3. **Identité CPU XAudio** : plus petit verrou vers un premier render audio sans trap.
4. Après progression du title, relancer les censuses de reachability : le producteur renderer ou les initialisations audio peuvent devenir atteignables naturellement.

# Audit adversarial

- Atteindre `0x820D32D0` prouve la vivacité du code, pas son nom métier.
- Une entrée intérieure `.pdata` n’est pas une nouvelle fonction.
- Les constantes `17` et `6` dans la même fonction ne prouvent pas leur passage comme arguments.
- Le callback renderer est le writer du gate, pas nécessairement le producteur de l’événement.
- Le descripteur XAudio nul est sélectionné par processeur ; sa cause scheduler reste à confirmer dans le même run.
- Un atlas de vtables d’un autre build ne peut pas nommer une classe PAL par adresse brute.
- Aucun run Xenia, CTest ou modification du runtime n’est revendiqué dans cette passe statique.
- Aucun ZIP ni octet propriétaire n’est publié avec ce rapport.

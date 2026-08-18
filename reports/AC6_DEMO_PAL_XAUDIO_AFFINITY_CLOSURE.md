# AC6 démo PAL — fermeture du descripteur XAudio nul et de l'affinité guest

Date : 2026-08-18  
Cible : démo campagne PAL Xbox 360, `Default.xex`  
XEX SHA-256 : `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`  
Image mémoire SHA-256 : `b81941994944b84f5792fd7b89cd698ca429b13c1bb4f501ea12e49dc54c2f01`

## Verdict

Le premier callback XAudio entraîné par le bridge choisit un descripteur selon :

```text
current_processor = *(uint8_t *)(r13 + 0x10C)
```

Le bridge natif actuel :

1. crée un unique bloc PCR/TLS `kGuestThreadBase` ;
2. écrit `0` dans `kGuestThreadBase+0x10C` ;
3. affecte le même `r13 = kGuestThreadBase` à toutes les fibers ;
4. accepte `KeSetAffinityThread` sans mémoriser le masque demandé ;
5. appelle le callback XAudio en modifiant seulement
   `current_guest_thread_id`, sans modifier le byte PCR.

Le callback synthétique voit donc le processeur `0`.

Or l'objet audio observé possède des descripteurs uniquement pour les
processeurs 4 et 5. Le sélecteur prend la paire du processeur 0, obtient
`nullptr`, puis le décodeur déréférence ce pointeur.

La chaîne du défaut natif est fermée :

```text
affinité demandée mais oubliée
+ PCR unique fixé à CPU 0
→ sélection de la paire CPU 0
→ descripteur nul
→ fault dans 0x8234F950
```

## 1. Correction d'identité

Le rapport `AC6_DEMO_XAUDIO_CLIENT_IS_A_MODE_TASK.md` de `main` attribue
l'objet à `CModeTaskRanking` en traitant `0x82065374` comme début de vtable et
`0x820653BC` comme son slot `+0x48`.

Cette attribution ne résiste pas aux bytes PAL exacts.

### RTTI PAL de `CModeTaskRanking`

```text
type descriptor          0x82391A48
primary COL              0x82072DF4, offset 0
primary vtable           0x8200E8EC
secondary COL            0x82072E4C, offset 0x68
secondary vtable         0x8200E88C
```

Les pointeurs vers les COL se trouvent immédiatement avant les deux vtables :

```text
[0x8200E8E8] = 0x82072DF4
[0x8200E888] = 0x82072E4C
```

À l'inverse :

```text
[0x82065370] = 0x82351340
[0x82065320] = 0x823538E0
```

Ce sont des pointeurs de fonctions, pas des COL. Les zones `0x820653xx`
forment plusieurs tables audio contiguës mêlées à quelques constantes ; elles
ne constituent pas une vtable Ranking unique.

### Objet audio global

Le constructeur PAL :

```text
demo:0x82355F70
```

écrit successivement :

```text
0x820653A8
0x820653BC
```

dans l'objet, puis le publie à :

```text
[0x829DA528] = this
```

Cette provenance par constructeur est plus forte qu'une attribution par
« vtable la plus proche » importée d'un autre espace d'adresses.

Le nom minimal conservé est :

```text
global audio engine / XAudio render-client object
```

et non `CModeTaskRanking`.

Les adresses de vtable ne sont pas des noms de famille. Les adopter par
proximité donne exactement le genre de parenté qui finit devant un juge.

## 2. Objets audio

### Moteur audio global

```text
constructeur             0x82355F70
initialiseur             0x82356070
table finale             0x820653BC
global                   0x829DA528
callback enregistré      0x8236DD98
dispatcher callback      0x82355E58
```

`0x8236DD98` ignore ses arguments externes, charge le global, puis
tail-branche vers `0x82355E58`.

### Source voice

```text
constructeur             0x823587B0
table                    0x820653F8
méthode de traitement    0x82357FC8, slot +0x54
```

### Enfant décodeur ou processeur

```text
factory                  0x82354AB0
constructeur             0x82354280
taille                   0x114
table                    0x82065320
méthode de traitement    0x82354390, slot +0x18
```

Le trap observé possède `r26` dans cet objet enfant. Un ancien `r27` conservé
par l'appelant peut encore pointer vers le moteur global sans devenir le
`this` du callee. Les registres préservés PPC ne sont pas des badges nominatifs.

## 3. Table des descripteurs par processeur

`0x82356070` construit deux descripteurs pour chaque processeur de sortie
activé :

| CPU | Descripteur A | Descripteur B |
|---:|---:|---:|
| 0 | `global+0x0C` | `global+0x10` |
| 1 | `global+0x14` | `global+0x18` |
| 2 | `global+0x1C` | `global+0x20` |
| 3 | `global+0x24` | `global+0x28` |
| 4 | `global+0x2C` | `global+0x30` |
| 5 | `global+0x34` | `global+0x38` |

La factory est :

```text
0x8234FCB0
```

Chaque allocation mesure 36 octets et expose son interface publique à
`allocation+8`. Le constructeur écrit une fréquence de 48 000 Hz.

La même boucle crée les workers audio avec des masques d'affinité one-hot :

```text
1 << processor
```

Les traces existantes autour du démarrage audio comprennent les masques :

```text
0x10
0x10
0x20
```

soit les processeurs 4, 4 et 5.

## 4. Sélection et propagation du null

La chaîne est :

```text
0x82357FC8
→ 0x8236F7C8
→ vslot +0x18 de l'enfant
→ 0x82354390
→ 0x8234F950
```

À `0x8236F810` :

```powerpc
lbz r9, 0x10C(r13)
```

La fonction sélectionne ensuite :

```text
global + 0x0C + 8 * processor
```

et peut échanger A/B selon le handle source. Elle appelle enfin :

```text
r3 = enfant décodeur
r5 = interface descripteur
vslot +0x18
```

`0x82354390` effectue :

```cpp
descriptor_base = r5 != 0 ? r5 - 8 : 0;
sub_8234F950(descriptor_base, &local_format);
```

`0x8234F950` lit :

```text
base+0x14
base+0x18
base+0x1C
```

Le chemin n'a donc aucune sémantique valide pour `r5 == nullptr`.

## 5. Défaut du bridge natif

### PCR unique

`GuestBridge::prepare` initialise :

```text
kGuestThreadBase+0x10C = 0
```

`GuestBridge::initialize_guest_fiber` donne à toutes les fibers :

```text
r13 = kGuestThreadBase
```

Toutes lisent donc le même byte de processeur.

### Affinité oubliée

Le handler `KeSetAffinityThread` :

- valide que le masque est non nul ;
- valide l'objet thread ;
- écrit `1` dans le pointeur de valeur précédente ;
- retourne le succès ;
- ne mémorise pas le nouveau masque ;
- ne modifie pas le PCR du thread.

### Callback synthétique

`dispatch_xaudio_frame` copie le contexte primaire puis modifie uniquement :

```text
current_guest_thread_id = 2
```

Il ne modifie ni `frame.r13`, ni :

```text
[frame.r13+0x10C]
```

Le code invité lit donc toujours `0`.

Le dispatcher d'interruption graphique possède déjà le patron correct : il
sauvegarde le byte PCR, écrit le processeur actif, exécute le callback, puis
restaure le byte. XAudio n'applique pas ce contrat.

## 6. Jointure avec l'objet observé

La trace du client au premier callback montrait :

```text
+0x0C..+0x28 = 0
+0x2C et suivants = pointeurs actifs
```

Le layout statique traduit cela exactement :

```text
CPU 0..3  aucune paire
CPU 4..5  paires construites
```

Le bridge force CPU 0, et le guest sélectionne donc `global+0x0C`, nul.

Cette jointure ne dépend pas du nom de classe et ne demande aucune attente
supplémentaire. Attendre plus longtemps avec le même processeur synthétique
choisit simplement le même pointeur nul avec davantage de patience.

## 7. Contrat d'implémentation

La correction native doit :

1. mémoriser le masque one-hot demandé pour chaque `GuestThread` ;
2. rejeter les masques nuls, multi-bits ou hors des six CPU ;
3. dériver l'indice 0..5 ;
4. publier cet indice dans le PCR visible de la fiber ;
5. sauvegarder et restaurer le byte autour des callbacks synthétiques ;
6. associer le callback XAudio à un processeur audio qualifié ;
7. vérifier que le descripteur sélectionné est non nul avant l'appel.

Il ne suffit pas d'écrire `4` parce que cela évite le crash. Le processeur du
callback driver doit être dérivé d'un contrat observé ou de son worker associé.
Sinon, nous remplacerions un défaut déterministe par un heureux hasard doté
d'une constante.

## 8. Discriminant runtime minimal

Une seule ligne de trace par callback suffit :

```text
tick
thread guest
masque d'affinité conservé
r13
byte [r13+0x10C]
paire de descripteurs sélectionnée
r5 final
```

Critère :

```text
processor(byte) == processor(affinity)
ET
descripteur sélectionné != null
```

## Verdict

| Claim | État |
|---|---|
| sélection par `r13+0x10C` | **fermée A** |
| six paires de descripteurs | **fermées A** |
| propagation du null | **fermée A** |
| bridge partage un PCR CPU 0 | **fermé A** |
| bridge oublie l'affinité | **fermé A** |
| attribution `CModeTaskRanking` | **réfutée sur le PAL exact** |
| cause du défaut du bridge | **fermée A-** |
| CPU exact du callback XAudio driver | **ouvert, trace bornée** |

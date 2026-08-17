# AC6 démo — résultats campagne, rangs, galerie et sauvegarde

Date : 2026-08-17  
Méthode : analyse statique PPC et reconstruction de structures  
Convention : toutes les adresses `demo:` appartiennent au XEX de la démo

## Résumé exécutif

Le pipeline post-mission est maintenant borné :

```text
objectifs / opérations
→ rangs ordinaux et récompenses
→ record mission de 400 octets
→ agrégat campagne
→ progression galerie et déblocages
→ codec SAVE v5
→ trois profils persistants
```

Une correction importante s'impose : `CModeTaskRanking` n'est pas le classement
de la campagne. Il appartient aux classements Xbox Live. Le résultat de campagne
est affiché par les classes `CModeTaskGalleryCampaignResult*` et les résultats
mission par `CModeTaskFreeMissionSelect`.

## 1. Record mission de 400 octets

Chaque profil contient seize records de `0x190` octets, indices 0 à 15. Les
missions jouables utilisent 1 à 15 ; l'index 0 est conservé sans lui attribuer
un rôle métier non démontré.

Champs fermés :

| Offset | Rôle |
|---:|---|
| `+0x000` | rang/état global |
| `+0x004..+0x01C` | sept rangs/états d'opération |
| `+0x0D8` | score de mission |
| `+0x0DC..+0x0F4` | sept récompenses ou scores d'opération |
| `+0x0F8` | bonus de rang global |
| `+0x0FC` | récompense de l'opération principale |
| `+0x100` | durée totale de mission |
| `+0x104..+0x11C` | sept durées ou deltas temporels |
| `+0x120` | indice de catégorie, plage 0..6 |
| `+0x144..+0x18C` | dix-neuf compteurs additifs |

Les queries de l'interface Free Mission lisent directement ces champs.

## 2. Calcul des rangs

`demo:0x8220F780` traite les records runtime d'objectif, stride 68.

Deux chemins existent.

### Seuils directs

Trois seuils donnent les grades 1 à 4 :

```text
>= seuil 0  → 1
>= seuil 1  → 2
>= seuil 2  → 3
sinon       → 4
```

### Agrégation

Pour un objectif composé, le moteur transforme chaque rang en `6-rang`, en
calcule la moyenne, applique un arrondi, puis reconvertit le résultat. Ce chemin
produit les valeurs 1 à 5.

Contrat statique :

```text
1 = meilleur
5 = moins bon rang valide
6 = sentinelle / non classé
0 = absent
```

L'association `1=S, 2=A, 3=B, 4=C, 5=D` est très probable et cohérente avec
la documentation du jeu, mais la lettre D n'est pas récupérée comme chaîne
source dans le binaire. Elle reste donc une étiquette de grade B+, non une
preuve ABI.

## 3. Score

Le finaliseur `demo:0x822771A0` emploie la table :

```text
index : 0    1     2    3    4    5    6
bonus : 0, 1000, 800, 600, 400, 200, 200
```

Puis :

```cpp
record.score =
    MissionManager.accumulated_score
  + rank_bonus[rank_index];
```

Le bonus est également conservé séparément à `record+0xF8`.

Une seconde progression applique :

```cpp
category_delta =
    trunc(record.score * multiplier[record.category_index]);
```

avec :

```text
[0.0, 0.5, 1.0, 1.5, 2.0, 3.0, 4.0]
```

Le score final n'est donc pas une simple somme de destructions. Il combine le
score accumulé par la mission, le rang global et une projection vers des
catégories persistantes.

## 4. Agrégat campagne

Le merge `demo:0x822CFFB0` :

- ajoute la durée à `aggregate+0x08` ;
- incrémente le nombre de missions à `+0x10` ;
- additionne 19 compteurs à `+0x14..+0x5C` ;
- conserve plusieurs meilleurs rangs et meilleurs scores ;
- met à jour des tables indexées par catégorie ;
- incrémente `aggregate+0x04` lorsque la mission 15 est finalisée.

Les classes `CModeTaskGalleryCampaignResult*` lisent cet agrégat pour afficher
durée totale, compte de missions, ratios et totaux.

## 5. Galerie

### Assault Records

Le profil contient 45 entrées de huit octets :

```text
+0  acquired/unlocked
+1  new/unseen
+4  valeur ou compteur auxiliaire
```

L'interface lit séparément les deux bytes et peut acquitter le marqueur `new`.

### Médailles

Le profil contient 54 entrées de deux octets :

```text
+0  acquired
+1  new/unseen
```

Lors de l'ouverture de la galerie, les médailles acquises peuvent perdre leur
marqueur `new`.

### Mise à jour atomique

Après la copie du record mission et son merge dans l'agrégat, le finaliseur
appelle successivement quatre fonctions de progression :

```text
0x82276B38
0x82276EF8
0x822764D0
0x82276798
```

Elles mettent à jour résultats dérivés, Assault Records, médailles et lattice
de déblocage. La galerie n'est donc pas recalculée paresseusement à chaque
affichage : son état est persistant.

## 6. Profil runtime

Définissons :

```text
B = active_root + bank_index * 0xAAB8
P = B + 0x6BC
```

Alors :

```text
P+0x0038  16 records mission × 400
P+0x1938  agrégat / galerie
P+0x63B4  second grand bloc de progression
P+0xAA38  tail de 128 octets
```

Le runtime expose quatre fenêtres `B`, index 0 à 3.

## 7. Codec SAVE v5

`ACE6::CAce6SaveProperty` possède un codec virtuel complet :

```text
reset        0x8229FE08
serialize    0x8229FEA8
deserialize  0x8229FF38
size         0x8229FFC0
validate     0x822A0028
```

Validation :

```text
'S' 'A' 'V' 'E'
version = 5
selector ∈ [0,3]
tous les enfants valides
```

Le payload sérialisé mesure exactement :

```text
0x2009C = 152 + 3 × 0xAAAC
```

Il contient trois profils persistants. Chaque profil sérialise :

```text
16 bytes
36 bytes
16 × 400 bytes de résultats mission
19064 bytes de galerie/agrégat
18048 bytes de progression secondaire
128 bytes de tail
```

La taille mémoire du profil est `0xAAB8`, soit douze octets de plus que son
payload. Ces douze octets correspondent aux trois pointeurs de vtable omis par
le codec.

Le binaire contient les noms :

```text
ace6
save.dat
%s%04d.dat
replay.dat
```

Le `SaveProperty` est embarqué dans la tâche de stockage à `task+0x190`, qui
pilote les opérations asynchrones de lecture et d'écriture.

## 8. Quatre banques runtime, trois profils disque

Le runtime expose quatre banques de `0xAAB8`, mais le codec SAVE n'en écrit que
trois.

Ce résultat réfute l'ancienne lecture « une banque par difficulté ». La
difficulté est un autre paramètre et possède son propre chemin.

La meilleure borne actuelle est :

```text
3 banques persistantes
+ 1 banque runtime supplémentaire
```

Le rôle précis de la quatrième banque, scratch, guest, temporaire ou miroir,
reste ouvert. Il ne doit pas être inventé à partir du seul écart de cardinalité.

## 9. Front suivant

Le prochain sous-système recommandé est la progression d'inventaire et de
hangar :

```text
résultat mission / galerie
→ lattice de déblocage
→ avions, variantes, couleurs et armements disponibles
→ CSelectAircraftManager
→ profil sauvegardé
```

Les fonctions de progression écrivent déjà des bytes distincts pour les
missions 4, 5, 6, 8, 9, 10, 12, 13 et 15, puis évaluent une liste de quinze
identifiants. Il reste à joindre ces identifiants aux entrées d'avion, aux
couleurs, aux armes et aux contrôles du hangar.

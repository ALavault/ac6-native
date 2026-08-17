# AC6 démo — fermeture statique du navigateur de classements

Date : 2026-08-17  
Corpus : démo Xbox 360 `ACE6_X360`  
Méthode : analyse statique PPC et inspection des structures, sans exécution Xenia Edge  
Convention : toutes les adresses `demo:` appartiennent au XEX de la démo

## Résumé exécutif

`CModeTaskRanking` n'est pas le calculateur du rang obtenu à la fin d'une mission.
C'est le navigateur des **leaderboards hors ligne et en ligne** : choix du board,
filtres de mission/stage/difficulté, requête asynchrone Xbox, normalisation de dix
lignes par page et rendu de sept colonnes.

La chaîne est désormais bornée :

```text
CModeTaskRanking
  → sélection du board et des filtres
  → CX360RankingBoardApp
  → requête/stat records backend
  → résultats bruts de 48 octets
  → lignes UI normalisées de 72 octets
  → page de 10 lignes
  → callbacks et formatteurs de l'écran ranking
```

Une seconde structure de 116 octets appartient au chemin générique de construction
et de stockage des records de requête/statistique. Elle ne doit pas être confondue
avec les lignes de réponse de 48 octets, ni avec les lignes normalisées de 72 octets.
Le moteur utilise donc trois formats successifs, parce qu'un seul aurait été trop
reposant pour les gens arrivés vingt ans plus tard.

## 1. Identité RTTI et topologie

### `CModeTaskRanking`

```text
type descriptor     demo:0x82391A48
primary vtable      demo:0x8200E8EC
secondary vtable    demo:0x8200E88C
constructor         demo:0x821906B8
allocation          0x20718 octets
main update/HSM     demo:0x82190B78
```

Le constructeur installe :

```text
this+0x00  primary vtable
this+0x68  secondary/FSM vtable
this+0x70  helper embarqué construit par demo:0x822628A8
this+0x206C0 tuple de pagination initialisé à {0,0,1,0}
```

Il enregistre la chaîne :

```text
ランキング表示
```

soit « affichage du classement ».

### Classes backend

```text
CX360RankingBoard
ACE6::CAce6RankingBoardApp
CX360RankingBoardApp
```

Ces classes forment la couche de requête et de conversion entre les APIs Xbox et
les structures d'affichage AC6.

## 2. Preuve qu'il s'agit d'un navigateur de leaderboards

Les chaînes d'interface comprennent :

```text
OFFLINE
ONLINE
ロード中...     chargement...
受信中...       réception...
送信中...       envoi...
Ranking_EndProcess
```

Les familles de boards sont :

```text
campagne, score par mission et difficulté
campagne, score total par difficulté
nombre total de destructions
BATTLE ROYALE, par stage et total
TEAM BATTLE, par stage et total
CO-OP BATTLE, par mission
SIEGE BATTLE, par mission
```

Le sélecteur de board `demo:0x8218EF68` réduit l'état des filtres à six familles
internes. La couche backend `demo:0x821510C0` dérive ensuite le board, le stage,
la mission et la difficulté effectifs.

Les difficultés sont les cinq valeurs :

```text
ACE, EXPERT, HARD, NORMAL, EASY
```

## 3. Sept colonnes statiques

Une table à :

```text
demo:0x82391988
```

contient exactement sept descripteurs de 24 octets :

| ID | Colonne |
|---:|---|
| 1 | pays/région |
| 2 | score |
| 3 | destructions |
| 4 | temps |
| 5 | appareil utilisé |
| 6 | arme spéciale utilisée |
| 0 | Board ID |

Les six colonnes métriques portent le flag `0x01000000`. Le descripteur Board ID
utilise un contrat différent, avec deux champs activés à `1`.

`demo:0x8218F7B8` parcourt exactement les sept descripteurs et construit les
en-têtes ou colonnes correspondants.

## 4. Trois niveaux de records

### 4.1 Record générique de requête : 116 octets

`demo:0x82148730` alloue et initialise exactement :

```text
0x74 = 116 octets
```

`demo:0x821487B8` effectue toutes ses opérations de plage et d'index avec ce
stride. Ce record appartient au conteneur générique de requêtes/statistiques.

### 4.2 Ligne brute d'une réponse : 48 octets

Le normaliseur `demo:0x821505C8` parcourt les lignes de réponse avec :

```text
stride = 48 octets
```

Layout minimal :

| Offset | Rôle |
|---:|---|
| `+0x00` | identifiant Xbox de l'utilisateur, utilisé pour retrouver la ligne locale |
| `+0x08` | rang |
| `+0x10` | valeur backend 64 bits |
| `+0x18` | gamertag de 16 octets |
| `+0x28` | nombre de propriétés |
| `+0x2C` | pointeur vers les propriétés de 24 octets |

### 4.3 Ligne normalisée AC6 : 72 octets

Le même normaliseur produit un record de :

```text
0x48 = 72 octets
```

| Offset | Sémantique | Grade |
|---:|---|---|
| `+0x00` | rang / validité | A |
| `+0x08` | valeur backend 64 bits | B- |
| `+0x10` | gamertag | A |
| `+0x20` | statistique secondaire 64 bits, probablement kills ou métrique propre au board | C+ |
| `+0x28` | score ou valeur principale 64 bits | A- |
| `+0x30` | identifiant d'appareil | A- |
| `+0x34` | identifiant d'arme spéciale | A- |
| `+0x38` | temps en secondes | A |
| `+0x3C` | pays/région | A- |

Le score campagne est compacté avec plusieurs sous-champs. Le normaliseur sépare
les bits bas en :

```text
appareil
arme spéciale
temps
```

puis conserve la partie haute comme valeur principale. Le temps est transformé
par rapport à une borne de 3600 secondes.

La sémantique précise de `+0x20` reste ouverte. L'inventaire des colonnes en fait
un candidat naturel pour la statistique de destructions, mais le lien de propriété
n'est pas assez direct pour le promouvoir en grade A.

## 5. Pagination et ligne du joueur local

Le backend réserve :

```text
une ligne locale normalisée
10 lignes normalisées de liste
```

Le tableau de liste mesure donc :

```text
10 × 72 = 720 octets
```

`demo:0x821506E0` remet les dix lignes à zéro, normalise la réponse et publie le
nombre disponible. `demo:0x82150748` recherche séparément la ligne dont l'identité
Xbox correspond à l'utilisateur local.

Le navigateur ajuste le curseur par :

```text
±1 ligne
±10 lignes
```

avec bornage sur le nombre total de résultats.

## 6. Formatage des lignes

Les formats centraux sont :

```text
%3d) %d %s
%08d %02d:%02d'%02d"
%08d
%02d:%02d'%02d"
```

La première forme affiche :

```text
rang, pays/région, gamertag
```

Les fonctions dédiées :

```text
demo:0x8219B8A8  nom d'appareil
demo:0x8219B970  nom d'arme spéciale
demo:0x82191320  valeur 64 bits, format variant selon la famille de board
demo:0x82191390  rang/identité du joueur
demo:0x8218F8E0  ligne campagne avec temps
demo:0x8218FA58  ligne sans temps explicite
```

## 7. Contrat des callbacks UI

Les callbacks 940 à 944 décrivent une ligne de liste :

```text
940  rang
941  temps formaté
942  valeur principale 64 bits
943  identité rang + gamertag
944  clé composée appareil / arme / pays
```

Les callbacks 951 à 954 fournissent les mêmes familles pour la ligne du joueur
local.

Les callbacks 927 à 929 exposent :

```text
compte global ou disponible
nombre de lignes normalisées
page de 10 lignes
```

Ce contrat est suffisant pour reproduire l'écran sans conserver les structures
Xbox originales après normalisation.

## 8. Automates

### Automate écran

`demo:0x82190B78` utilise 16 états, numérotés de 0 à 15. Les cibles de branchement
sont entièrement extraites. Les états orchestrent :

```text
entrée et sortie de l'écran
choix offline/online
sélection de catégorie
sélection de mission/stage/difficulté
chargement/réception/envoi
navigation des résultats
fin du processus
```

Les noms internes de chaque état ne sont pas présents. Le graphe est fermé, la
nomenclature ne l'est pas.

### Automate backend

`demo:0x82151280` pilote un automate asynchrone 0..12, puis l'état terminal 14.
Il enchaîne :

```text
création de requête
résolution du board
publication du contexte utilisateur
émission et polling
téléchargement des lignes
normalisation
post-traitement
fermeture
```

Le code considère `997` comme statut asynchrone particulier et conserve un latch
de retry/pending. Les noms API exacts restent volontairement neutres tant que les
imports/ordinals n'ont pas été joints.

## 9. Correction : `Ranking` n'est pas le résultat de mission

Les classes suivantes sont distinctes :

```text
CModeTaskGalleryCampaignResult
CModeTaskGalleryCampaignResultMission
CModeTaskGalleryMedalMission
```

Le binaire contient également :

```text
AIRCRAFT RANK:
Aircraft Rank : Lv%d
```

Le calcul du résultat, des médailles et du niveau d'appareil appartient donc à
un autre sous-système. Aucun compteur du leaderboard ne doit être utilisé comme
preuve de sa formule.

Cette distinction corrige le choix annoncé au début de la passe : le sous-système
fermé ici est le **navigateur de classements**, pas le calcul de rang post-mission.

## 10. Projection native recommandée

La frontière native utile est :

```cpp
struct RankingRow {
    uint32_t rank;
    std::string gamertag;
    uint64_t primary_value;
    std::optional<uint64_t> secondary_value;
    uint32_t aircraft_id;
    uint32_t special_weapon_id;
    uint32_t time_seconds;
    uint32_t country_code;
};

struct RankingPage {
    BoardIdentity board;
    std::vector<RankingRow> rows; // max 10
    std::optional<RankingRow> local_player;
    uint32_t total_rows;
};
```

Les requêtes Xbox, les records de 116 octets et les pointeurs invités doivent
rester confinés à un adaptateur. Le produit natif ne gagne rien à reproduire les
contorsions de l'API originale après la conversion en lignes stables.

## 11. Frontières résiduelles

| Front | État |
|---|---|
| RTTI et ownership | fermé A |
| catalogue de boards | fermé A |
| sept colonnes | fermé A/A- |
| record de requête 116 octets | fermé A |
| résultat brut 48 octets | fermé A |
| ligne normalisée 72 octets | fermé A, un champ sémantique partiel |
| pagination 10 lignes | fermée A |
| automate écran | cibles fermées, noms partiels |
| automate backend | fermé structurellement |
| protocole/imports Xbox exacts | ouvert |
| formule de rang de mission | hors de ce sous-système |

## 12. Prochain sous-système

Le prochain front rationnel est :

```text
CModeTaskGalleryCampaignResultMission
+ CModeTaskGalleryMedalMission
+ producteurs AIRCRAFT RANK
        ↓
statistiques de combat et de mission
        ↓
score, médaille, rang et niveau d'appareil
        ↓
progression campagne et sauvegarde
```

Le débriefing temporel, le leaderboard et la galerie de résultats sont maintenant
séparés. Il devient possible d'attaquer la formule de résultat sans mélanger trois
écrans dont le seul crime commun est d'afficher des nombres après une mission.

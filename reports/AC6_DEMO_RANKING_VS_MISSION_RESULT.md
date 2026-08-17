# Correction de frontière — leaderboard, débriefing et résultat de mission

## Ancienne hypothèse

Le prochain sous-système avait été formulé comme « classement post-mission » en
partant de `CModeTaskRanking`.

## Correction

`CModeTaskRanking` est un navigateur de leaderboards offline/online :

```text
boards campagne et multijoueur
requêtes asynchrones Xbox
pagination
ligne du joueur local
gamertags
```

Le résultat de mission appartient à des classes distinctes :

```text
CModeTaskGalleryCampaignResult
CModeTaskGalleryCampaignResultMission
CModeTaskGalleryMedalMission
```

Le débriefing constitue encore un troisième sous-système : replay temporel,
caméra et tracks d'entités.

## Séparation finale

```text
Debriefing
    replay tactique

Ranking
    consultation de leaderboards

CampaignResult / MedalMission
    résultat, médaille et progression post-mission
```

Aucune formule de score ou de rang n'est déduite des lignes du leaderboard.

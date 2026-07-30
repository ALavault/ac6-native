# Cycle 342 — l'oracle tourne en headless, et il montre qu'il manque une couche d'interface entière

## 1. Oracle établi, sans toucher au bureau de l'opérateur

Le `XENIA_WINE_ORACLE_HANDOFF.md` prescrit « la session GNOME vive ». C'est
contraire à la consigne permanente de l'opérateur. Le lanceur lit en fait
`DISPLAY` et `XAUTHORITY` de l'environnement, il n'impose rien : pointé sur un
serveur X headless, il fonctionne.

```text
preflight            status=ready release=16e1eb8 renderer=vulkan
affichage            Xvfb :85, 1280x720x24, avec cookie MIT-MAGIC-COOKIE-1
service              ac6-xenia-wine-gui.service  active
processus            xenia_canary.exe  vivant
capture              738 Ko, écran-titre complet
```

Deux pièges, tous deux mesurés :

- Xvfb doit être lancé **avec `-auth`** et un cookie réellement enregistré ;
  un `XAUTHORITY` pointant sur un cookie que le serveur ignore fait échouer le
  pilote X11 de Wine (`nodrv_CreateWindow`, « explorer process failed to
  start »), ce qui **ressemble à un défaut Vulkan et n'en est pas un** ;
- un verrou `/tmp/.X85-lock` périmé empêche Xvfb de démarrer, silencieusement.

Le cycle 316 concluait « Xenia sous Xvfb ne rend rien ». Cette conclusion visait
la build **Linux native**, que le handoff décrit lui-même comme restant noire.
La route **Windows sous Wine** rend parfaitement en headless. La contrainte
n'était pas l'affichage.

Profil et sauvegarde vérifiés intacts après l'exécution.

## 2. Le résultat : bien plus qu'une chaîne manquante

Oracle, même point du jeu (titre -> Start -> A -> A) :

```
                      ) GAME DATA
   +--------------------------------------------------+
   |                    FILE 01                        |
   | MISSION              ----                         |
   | DIFFICULTY LEVEL     ----                         |
   | CAMPAIGN FLIGHT TIME ----:--:--                   |
   |            ... FILE 02, FILE 03 ...               |
   +--------------------------------------------------+
                  Load file 01?
             [ YES ]        [ NO ]
                              (A) OK   (B) CANCEL
```

Notre runtime, au même point : **les boutons OUI/NON seuls**, sur un panneau
vide. Manquent :

| élément | oracle | nous |
|---|---|---|
| question « Load file 01? » | présente | **absente** |
| navigateur GAME DATA (FILE 01/02/03) | présent | **absent** |
| libellés MISSION / DIFFICULTY / FLIGHT TIME | présents | **absents** |
| pied « (A) OK / (B) CANCEL » | présent | **absent** |
| boutons YES / NO | présents | présents |

**Ce n'est pas une chaîne qui manque, c'est une couche d'interface entière.**
Seuls les deux boutons survivent.

## 3. Ce que cela réécrit

Le cycle 337 posait « le dialogue n'a pas de texte » et les cycles 338-341 ont
cherché la résolution d'**une chaîne**. La cible était trop étroite : sept
candidats éliminés l'ont été pour la bonne raison mais sur le mauvais objet.

Cela explique aussi les boutons inertes sans hypothèse supplémentaire : c'est un
**écran de chargement de sauvegarde**, et confirmer par A charge le fichier 01.
Si la liste des fichiers n'est pas construite, il n'y a rien à charger, et A n'a
rien à faire. Un seul défaut, deux symptômes — comme le cycle 337 le
soupçonnait, mais un cran plus haut que l'objet du dialogue.

## 4. Front suivant, resserré

La question n'est plus « d'où vient cette chaîne » mais **« pourquoi cette
couche d'interface n'est-elle pas dessinée »**. Elle est faite de texte et de
cadres, là où le fond et les deux boutons passent : c'est donc un sous-système
de dessin, ou un jeu de ressources, sélectivement absent.

1. Comparer les compteurs de dessin hôte entre les deux points de l'oracle et
   les nôtres — la couche manquante devrait apparaître comme des primitives
   absentes, pas comme des primitives invisibles.
2. L'oracle est désormais **disponible et reproductible en headless** : toute
   hypothèse visuelle peut être tranchée en une exécution. C'est l'acquis
   principal de ce cycle.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

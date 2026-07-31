# Cycle 414 — l'oracle ne démarre pas : `CreateWindowExW failed`

## 1. Tentative

Comparaison à l'oracle Xenia sur l'écran bloqué — la piste laissée de côté
depuis plusieurs cycles et la seule qui dirait ce que le jeu est *censé* faire
là.

Montage conforme à ce qu'établissaient les cycles antérieurs : Xvfb démarré
**avec `-auth`** et un vrai cookie `xauth`, `XAUTHORITY` exporté, Wine lancé
depuis le répertoire de l'application.

## 2. Échec

```
!> CreateWindowExW failed
!> Failed to open the platform window
!> Failed to create the main emulator window
i> Cheap-skate exit!
```

Wine n'a pas pu créer de fenêtre sur cet affichage. Aucune image : la capture
fait 250 octets, soit un écran vide.

**Aucune comparaison n'a eu lieu.** Rien n'est conclu de cet essai, et le
`.png` vide ne doit pas être pris pour un résultat.

## 3. Piste pour la reprise

Le cookie `xauth` était présent et l'affichage fonctionnel — le runtime natif y
tourne sans difficulté. L'échec est donc propre à Wine, pas à Xvfb. Deux
vérifications évidentes, non faites ici faute de contexte :

- le préfixe Wine est-il initialisé pour cet utilisateur et cet affichage
  (`wineboot`) ?
- `DISPLAY` et `XAUTHORITY` traversent-ils bien jusqu'au processus Wine, ou
  sont-ils perdus par l'invocation depuis un sous-shell ?

## 4. Où en est le dossier

Rien n'a changé sur le fond depuis le cycle 407 : la couche hôte est mesurée
saine dans son entier, la couche manette aussi (cycles 408-413), et le défaut
est dans la logique d'interface invitée, non instrumentée à ce jour.

Les cycles 408 à 414 n'ont pas rapproché de la mission 1. Ils ont écarté une
couche et corrigé trois erreurs d'instrument. C'est un résultat, mais ce n'est
pas le livrable.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

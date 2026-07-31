# Cycle 375 — seules les textures fautives empruntent ce chemin de chargement

## 1. Mesure

Rang de chargement des sept textures de la passe, journalisé **sans
échantillonnage** (toutes les sept sont dans le filtre depuis le cycle 370) :

```
load #2537  base=028B7000  320x180   NE REND PAS
load #2548  base=03514000  256x256   NE REND PAS
```

**Les cinq textures qui s'affichent n'apparaissent pas du tout.**

Ce n'est pas un biais d'échantillonnage : le filtre journalise ces sept bases
inconditionnellement, quel que soit le rang. Leur absence est donc un fait.

## 2. Inversion du tableau

Depuis le cycle 362, l'enquête compare « deux textures fautives » à « cinq
textures qui fonctionnent » en supposant qu'elles suivent le même chemin. **Elles
ne le suivent pas.**

- les deux fautives passent par `LoadTextureDataFromResidentMemoryImpl` ;
- les cinq fonctionnelles n'y passent **jamais** — elles atteignent le GPU
  autrement.

Cela retourne l'interprétation de vingt et une éliminations : les comparaisons
des cycles 362 à 374 opposaient des propriétés **statiques** identiques (format,
constante de fetch, disposition, dispatch), mais elles ne pouvaient pas révéler
que le chemin lui-même diffère, parce qu'aucune sonde ne mesurait *quel* chemin
était emprunté.

Le chemin de chargement depuis la mémoire résidente n'est donc pas « le chemin
commun où deux textures échouent » : c'est **le chemin qu'empruntent exactement
les deux qui échouent**.

## 3. Ce que cela ne dit pas encore

Deux lectures, non départagées :

1. ce chemin est défectueux, et toute texture l'empruntant sort vide ;
2. ce chemin est sain, mais il n'est emprunté que par des textures qui, pour une
   autre raison, ne sont pas prêtes au moment du chargement.

La première est plus économique. Elle se teste : si **toute** texture passant par
ce chemin échoue à s'afficher, le chemin est en cause ; s'il en existe une qui
l'emprunte et rend correctement, il ne l'est pas.

Cette question n'a jamais été posée — les sondes précédentes se limitaient aux
sept textures de la passe.

## 4. Front suivant

Journaliser **toutes** les textures passant par
`LoadTextureDataFromResidentMemoryImpl`, sans restriction aux sept, et croiser
avec ce qui est visible à l'écran. Le cycle 352 avait compté ~3 000 passages :
si les textures visibles du jeu y figurent en masse, le chemin est disculpé et
la cause est ailleurs ; si les passages sont rares et corrélés à des éléments
manquants, le chemin est la cause.

C'est la première fois depuis vingt cycles qu'une mesure change la **forme** de
la question plutôt que d'éliminer un candidat.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

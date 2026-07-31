# Cycle 394 — aucune des deux textures n'était fautive

## 1. Les deux tests d'omission, avec témoins verts

Navigation pilotée par le détecteur d'écran (cycle 393), écran de sauvegarde
atteint à l'itération 7 dans les deux cas, omission déclenchée 4 fois.

| texture omise | ce qui disparaît | conclusion |
|---|---|---|
| `0x03514000` (256x256) | **« YES » et « NO »** | elle dessine le texte des boutons |
| `0x028B7000` (320x180) | **le panneau de fond** | elle dessine le fond |

**Les deux rendent correctement.** Leur omission efface quelque chose de visible,
ce qui n'est possible que si elles s'échantillonnent bien.

## 2. Ce que cela renverse

La prémisse posée au cycle 362 — « ces deux textures s'échantillonnent à zéro »
— est **fausse**. Elle n'avait jamais été mesurée : elle avait été *déduite* du
fait que les lots multi-quads devaient être le texte manquant, déduction
elle-même jamais vérifiée (mise en garde du cycle 385).

Les cycles 362 à 384 ont donc mesuré, avec soin et témoins, les propriétés de
deux textures **qui fonctionnent**. Ces mesures restent exactes ; leur objet
était le mauvais. Vingt-sept « éliminations » n'éliminaient rien, faute de
défaut à éliminer.

## 3. Ce qui reste vrai, et ce que cela implique

L'écran de sauvegarde ne comporte qu'**une seule passe de contenu**
(`472913F460D4B446 / 8F1C48BA92C8E43E`, cycle 347), et cette passe dessine le
fond, le panneau et les boutons — tout ce qui est visible, et rien d'autre.

Le navigateur GAME DATA, les libellés MISSION / DIFFICULTY / FLIGHT TIME,
« Load file 01? » et le pied « (A) OK / (B) CANCEL », tous présents chez
l'oracle (cycle 342), ne sont dessinés par **aucune passe**.

**Ils ne sont pas mal rendus : ils ne sont jamais soumis.** Le défaut est dans la
logique invitée qui construit cet écran, pas dans le rendu.

C'est la direction qu'avait prise le cycle 367 — puis que j'ai retirée au
cycle 368 sur la foi de la prémisse fausse. Elle était juste.

## 4. Ce que la méthode retient

Une déduction non mesurée a survécu trente-deux cycles et a détourné tout
l'effort vers le sous-système graphique, qui était sain. Les témoins de vivacité
ont correctement protégé chaque mesure individuelle ; aucun ne pouvait protéger
contre une **cible** erronée.

**Vérifier ce qu'on observe avant de mesurer comment il se comporte.**

## 5. Front suivant

Reprendre au niveau invité : quelle fonction construit le contenu de l'écran de
sauvegarde, et pourquoi n'émet-elle que le dialogue ? L'oracle headless est
disponible pour comparer, et le détecteur d'écran rend la navigation
reproductible.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

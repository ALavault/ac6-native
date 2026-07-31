# Cycle 395 — notre runtime n'a aucun stockage de contenu ; l'oracle en a un

## 1. Observation

Après le renversement du cycle 394 — le rendu est sain, l'écran de sauvegarde
n'émet simplement pas le navigateur GAME DATA — la question redevient : que
construit l'invité, et sur quelles données ?

```
répertoire content à côté du binaire      : absent
répertoire content dans l'arbre référence : absent
```

L'oracle, lui, dispose d'une racine de sauvegarde documentée :

```
.tools/xenia-canary-windows/16e1eb8/app/content/E030000042B27D70/4E4D07D1/00000001/sav_acecombat6/
```

**L'oracle a des données de sauvegarde ; nous n'en avons aucune, ni même le
répertoire.**

## 2. Hypothèse, et sa limite

Un navigateur de fichiers de sauvegarde construit sa liste à partir du contenu
énuméré. Sans racine de contenu, `XamContentCreateEnumerator` peut échouer ou
rendre zéro élément, et l'invité peut alors ne rien construire — ce qui
correspond exactement à ce qui est observé : aucun dessin pour ces éléments.

**Limite honnête :** la capture de l'oracle montre `FILE 01` avec
`MISSION ----`, `DIFFICULTY ----`, `CAMPAIGN FLIGHT TIME ----:--:--`, donc des
emplacements **vides** qui sont pourtant dessinés. Un simple « pas de
sauvegarde » n'expliquerait donc pas l'absence totale du cadre.

La différence pertinente n'est peut-être pas « zéro sauvegarde » mais **« aucun
périphérique de contenu monté »** — un état que la console ne connaît pas et que
le jeu peut ne pas savoir représenter.

## 3. Test, simple et bon marché

Créer l'arborescence de contenu attendue et relancer :

```
<binaire>/content/<XUID>/4E4D07D1/00000001/
```

- si le navigateur apparaît, la cause est l'absence de stockage, et le correctif
  est de provisionner une racine de contenu — pas de toucher au rendu ;
- s'il reste absent, l'invité n'atteint pas ce code pour une autre raison, et il
  faut instrumenter la construction de l'écran côté invité.

Le détecteur d'écran du cycle 393 rend la vérification reproductible, et
`XamContentCreateEnumerator` est déjà journalisé.

## 4. Ce que ce cycle vaut

Il ne prouve rien. Il relève une différence **structurelle** entre notre
environnement et l'oracle, que trente-deux cycles d'investigation graphique
n'auraient jamais pu révéler, et propose le test le moins cher qui la départage.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

# Cycle 360 — la passe défaillante peint bien l'écran ; ce sont ses échantillons qui sont nuls

## 1. Mesure

Forçage de l'échantillon de texture à 1.0, **restreint** au shader
`8F1C48BA92C8E43E` (restriction prouvée vivante au cycle 359). Rafale de huit
captures espacées de 2,5 s après avoir atteint l'écran de sauvegarde :

```
f1..f8  moyenne RVB [173.2, 179.1, 173.0]  26 couleurs   (identiques)
```

**L'écran devient blanc et le reste.**

La rafale répond au problème de synchronisation qui avait fait échouer les trois
tentatives précédentes : plus besoin de viser une fenêtre temporelle.

## 2. Pourquoi c'est un résultat, et non le lavis du cycle 357

Au cycle 357 le forçage était **global** : tout blanchissait, y compris la passe
de présentation, et le test ne disait rien.

Ici le forçage est **restreint à un seul shader**, et la preuve que la
restriction opère est double :

- le contrôle journalise `forcing white sample in ps=8F1C48BA92C8E43E` (cycle 359) ;
- **le même binaire** rend la cinématique d'attrait normalement — 42 183
  couleurs — quand l'écran de sauvegarde n'est pas atteint.

Le blanc n'apparaît donc que là où cette passe dessine.

## 3. Ce que cela établit

**Les dessins de la passe défaillante atteignent l'écran et le peignent.**
Forcés au blanc, ils le recouvrent. Donc :

- la couche n'est **pas** perdue à la rastérisation ;
- sa géométrie est correcte et couvre bien la zone attendue ;
- ce sont les **échantillons de texture** qui ne rendent rien là où le texte
  devrait apparaître.

Le fond et les boutons OUI/NON, dessinés par cette même passe, s'affichent
normalement en fonctionnement nominal : **au moins une** des textures liées
s'échantillonne correctement. Le défaut est donc **par texture**, non par passe.

Cela s'accorde avec le cycle 350 : six textures distinctes liées, toutes
`fmt=20` (`k_DXT4_5`), de dimensions variées.

## 4. Front suivant, resserré

La question n'est plus « pourquoi la couche est invisible » mais **« pourquoi
certaines de ces six textures s'échantillonnent à zéro et d'autres non »**.

1. Corréler, par dessin, l'adresse de base de la texture liée avec ce qui est
   visible — le fond et les boutons d'un côté, le texte de l'autre.
2. Comparer les descripteurs des deux groupes : dimensions, mips, mode
   d'adressage, signedness. Le cycle 350 les a journalisés ; il reste à les
   partitionner selon la visibilité.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

# Cycle 450 — l'écriture forcée ne déplace rien : l'hypothèse tombe

## 1. Le test que j'avais désigné

Cycle 449 : « écrire directement dans ces mots pour forcer l'échange. Si le
surlignage bascule, le lien est prouvé ; s'il ne bouge pas, l'hypothèse tombe. »

Exécuté.

## 2. Résultat

| grandeur | valeur |
|---|---|
| écart de la bande de boutons après échange forcé | **4,385** |
| navigation réelle (référence, cycle 421) | ~131 |
| bruit au repos | 2 à 4 |

L'écart est **au niveau du bruit**. Le surlignage ne bouge pas.

**`0x82A53428` et `0x82A5342C` ne sont pas l'état de sélection.** L'hypothèse
est morte, par le test que j'avais moi-même fixé, au cycle suivant.

## 3. Réserve sur cette exécution

`--ac6_performance_mode=true` supprime la journalisation, donc la ligne
`[ac6-force]` n'a pas pu être relue : je n'ai pas **vérifié** que l'écriture a
bien eu lieu. Le cvar était posé et le chemin de code ne dépend pas du journal,
mais c'est une déduction, pas une observation.

La reprise doit refaire ce test avec la journalisation active. En l'état, le
résultat est fortement indicatif et non définitif — je le note plutôt que de
conclure trop vite, une fois de plus.

## 4. Ce que ces deux mots sont, alors

Ils changent avec Droite, jamais avec A, et les forcer n'affecte pas
l'affichage. Ce sont donc des données **corrélées** à la navigation sans la
commander — un reflet, un cache, ou un état voisin mis à jour par le même code.

C'est utile négativement : cela écarte une piste et resserre les cinq blocs
restants du cycle 448, jamais examinés.

## 5. État

Le dispositif de balayage est bon et a produit six blocs candidats. Un a été
testé et écarté. Cinq restent :

`0x82860000`, `0x82870000`, `0x828C0000`, `0x82900000`, `0x82910000`

Le bloc `0x82910000` est particulier : quatre compteurs espacés de 36 octets,
passant de ~`0x2EF9` à ~`0x2FAD` — même pas de 36 octets que la table du bloc
écarté, ce qui suggère des structures parallèles d'éléments d'interface.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

# Cycle 452 — le décalage forcé ne change rien non plus, et l'écriture est vérifiée

## 1. Test

Décalage d'une position forcé sur les sept entrées du bloc `0x82A53Cxx`,
exactement comme l'invité l'avait fait de lui-même au cycle 451.

**Écriture confirmée par le journal** :

```
[ac6-force] list shift applied: 00000044,00000048,0000004C ... -> shifted
```

C'est la réserve du cycle 450 levée : cette fois, l'exécution de l'écriture est
observée et non déduite.

## 2. Résultat

| grandeur | valeur |
|---|---|
| écart de la bande de boutons | **1,330** |
| navigation réelle | ~131 |
| repos | 2 à 4 |
| candidat écarté au cycle 450 | 4,4 |

**1,33 est en dessous du bruit de repos.** Aucun effet.

Le motif de décalage n'est donc pas ce qui pilote l'affichage. Deuxième candidat
éliminé, cette fois sans réserve d'instrument.

## 3. Ce que deux éliminations propres établissent

Les données statiques qui changent avec la navigation sont **corrélées, non
causales**. Deux structures distinctes — un échange de deux mots (450), une
table décalée de sept entrées (452) — reproduisent fidèlement le geste de
l'invité, et aucune ne déplace un pixel quand on la force.

L'interprétation la plus simple : ces blocs sont des **copies en aval** — état
répliqué pour la sérialisation, l'audio, ou un système parallèle — tandis que
l'affichage se règle ailleurs, sur une valeur que le balayage n'a pas isolée.

## 4. La méthode est maintenant éprouvée

Balayage dense déclenché, descente au mot, écriture forcée, mesure par région
avec échelle calibrée, et vérification de l'écriture. La chaîne complète
fonctionne et rend des verdicts nets en un cycle chacun.

C'est l'outillage qui manquait pendant les quarante cycles précédents ; il
existe désormais. Mais il n'a pas encore trouvé la cible, et le livrable n'a pas
bougé.

## 5. Reste à examiner

Blocs relevés au cycle 448 et non encore testés : `0x82860000`, `0x82870000`,
`0x828C0000`, `0x82900000`, `0x82910000`.

Candidats précis déjà notés : `0x82870F94` et `0x82870FDC` (`2`→`1`),
`0x82916F20`/`24` (`2`→`3`, `1`→`2`) — de petites valeurs discrètes, la forme
attendue d'un indice de sélection.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

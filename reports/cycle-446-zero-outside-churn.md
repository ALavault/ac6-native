# Cycle 446 — capture déclenchée : zéro changement hors des blocs bruyants

## 1. Le dispositif, enfin synchrone

Les cycles 443-445 butaient sur un compromis : ampleur, densité et synchronie
avec l'appui, impossibles ensemble avec une somme périodique. La périodicité est
supprimée :

- référence dense sur 256 Mo, prise à cadence lente ;
- **re-calcul immédiat au front `DPAD_LEFT`**, détecté dans la sonde de fronts ;
- l'appui tombe entre les deux captures.

## 2. Résultat

```
[ac6-trig] --- 0 non-churn blocks changed across the press ---
```

**Aucun bloc** — hors les quatre exclus — ne diffère de part et d'autre de
l'appui. Cela inclut le bloc contenant l'objet écran `0xA3317DE0`, ce qui
recoupe le cycle 441.

## 3. Le défaut de ma propre conception

J'ai exclu `0xA33D0000`, `0xA1910000`, `0xA1900000` et `0xA18F0000` **parce
qu'ils changent à chaque trame**. Si le champ de sélection loge dans l'un
d'eux — ce qui est parfaitement possible, un bloc de 64 Ko contenant à la fois
des données vivantes et l'état de l'interface — alors l'exclusion masque
exactement ce que je cherche.

Le résultat « zéro » est donc **compatible avec deux situations opposées** :
le champ est hors de la plage, ou il est dans un bloc exclu.

Septième fois qu'une restriction d'instrument, ici justifiée par le bruit,
supprime le signal avec lui. La forme est constante : je réduis le champ
d'observation pour rendre la mesure lisible, et j'y perds la chose observée.

## 4. Ce que le dispositif vaut malgré tout

Il est correct et réutilisable : dense, synchrone, sans échantillonnage ni
rotation. Les trois défauts des cycles 443-445 sont réellement corrigés. Seule
l'exclusion pose problème, et elle se retire en une ligne.

## 5. Reprise, immédiate

Rejouer la même capture déclenchée **sans exclusion**, puis, pour les blocs
signalés, descendre au mot : comparer les 16 384 mots de chaque bloc suspect
entre référence et déclenchement. Le bruit par trame produira des différences,
mais un champ de sélection change **une fois** et reprend la même valeur au
retour — ce qui le distingue d'un compteur.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

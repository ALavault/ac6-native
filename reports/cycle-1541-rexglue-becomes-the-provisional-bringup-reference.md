# Cycle 1541 — RexGlue devient la référence provisoire de bring-up

## Décision

La verticale Mission 01 peut désormais utiliser une implémentation RexGlue
épinglée comme approximation raisonnable du comportement Xbox 360 avant sa
qualification retail détaillée. Le but est de porter et d'intégrer rapidement
le cône M01, puis de qualifier seulement les sémantiques réellement atteintes.

Trois états sont obligatoires :

- `provisional-rexglue` : implémenté et atteint, sans contradiction connue ;
- `retail-qualified` : identité PAL et preuve bornée suffisantes pour une gate ;
- `divergent` : stub, absence, approximation connue ou résultat contradictoire.

Le premier état autorise le C++ manuscrit, les tests d'intégration et le replay
diagnostique. Il n'autorise ni la fermeture d'une lane, ni la publication, ni
une affirmation d'exactitude retail. Le produit reste indépendant de RexGlue,
de tout code PPC généré et des autres oracles.

## Garde de non-régression

Le contrat est scellé dans
`analysis/rexglue-semantic-trust-v1.json`. L'audit global vérifie son identité
PAL, ses transitions, ses interdictions et quatre divergences déjà établies :

- `dcbst` reconnu mais émis comme no-op par RexGlue 0.9 ;
- `frsqrte`, `vrefp` et `vrsqrtefp` remplacés par des mathématiques hôte exactes ;
- réservations `lwarx/ldarx` sans adresse ni granule ;
- `sync`, `lwsync` et `eieio` émis comme no-op ;
- `vmsum4fp128` réduit par `DPPS`, contrairement à l'ordre séquentiel du
  contrôle SLEIGH PAL actuel — sans promouvoir ce contrôle en preuve console.

Ces exceptions restent fail-closed. Les revues UnleashedRecomp,
Skate3Recomp, rexdex/recompiler, XenonRecomp, XenosRecomp et RexGlue peuvent
compléter le registre, mais aucune ne peut déclasser une divergence sans preuve
PAL bornée.

## Effet sur le plan

Le bring-up oracle/replay et le port natif M01 passent avant la qualification
exhaustive. À la première exécution déterministe complète, la trace fournit le
census exact des services, instructions et états GPU atteints. Seul ce census
est ensuite requalifié contre le XEX PAL canonique et, lorsque nécessaire,
Xenia ou une micro-exécution ciblée. M02–M15 restent des régressions de lecteurs
partagés, pas un scope sémantique.

## Frontière de fermeture

M01-B à M01-F et JV/JP/JG conservent leurs critères actuels. Une phase ne passe
que si toutes ses dépendances atteintes sont `retail-qualified` ou prouvées sans
effet, si le replay est déterministe, et si le produit natif passe ses tests et
audits sans dépendance oracle.

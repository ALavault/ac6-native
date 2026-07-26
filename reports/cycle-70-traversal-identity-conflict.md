# AC6 — conflit d'identité du parcours historique `0x8226ECB0`

Date : 2026-07-16

## Statut : supersédé

Ce constat ne décrit que le projet `ace-combat-6-corrected`. Une relecture
ultérieure du projet `ace-combat-6` avec les octets PPC retail qualifie le
caller et le corps de `0x8226ECB0`; le corps du projet corrigé reste
incompatible. La décision et le contrat natif à utiliser sont documentés dans
`cycle-94-traversal-project-provenance-recheck.md`. Le présent rapport est
conservé comme provenance du conflit, et ne doit plus être interprété comme
une invalidation générale de l'identité retail.

## Identité et méthode

- cible : AC6 PAL `default.xex`, Xbox 360 ; pas Xbox One ; SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- projet : `ace-combat-6-corrected` ;
- lecture seule : `analyzeHeadless -noanalysis`, `DumpRange.java`, plage
  `0x8226ECB0..0x8226EEF0` ; sortie
  `.build/ac6-ghidra-cycle-70/traversal.log`.

## Constat

Le journal historique `reports/logs/entry9-frame-consumer.log` attribuait à
`0x8226ECB0` une boucle de collection et trois appels directs vers
`0x8222CCD0`, `0x8222B740` et `0x82227378`. Le projet corrigé donne à la même
adresse un corps incompatible : il commence par `cmpwi r3,0`, écrit une
structure de pas `0x44`, puis fait des dispatchs par tables sur les octets
`+0x2c` et `+0x2a`. La plage relue ne contient aucun de ces trois appels.

La provenance du journal historique est donc insuffisante pour conserver
`0x8226ECB0` comme identité confirmée du parcours. Il ne s'agit pas d'une
divergence dynamique : c'est un conflit statique entre deux vues Ghidra qui
doit être résolu avant toute intégration runtime.

## Correction de frontière

`Function8226ecb0TraversalEvidence` est désormais explicitement un modèle
historique non réconcilié : `historical_frame_call_claim=true` et
`corrected_project_body_reconciled=false`. Le code hôte et ses tests restent
comme banc d'investigation, mais ne décrivent plus une traduction retail de
`0x8226ECB0` et ne doivent pas être raccordés au binaire natif.

## Recherche de l'ancien triplet

Deux scans supplémentaires, toujours headless et en lecture seule, ne trouvent
aucun appel ou branche PPC directe vers ce triplet dans le projet corrigé :

- `FindDirectCallsTo.java` examine les flux désassemblés ;
- `FindPpcBranchesTo.java` décode également les encodages `b`/`bl` et
  `bc`/`bcl` bruts, indépendamment des xrefs Ghidra.

Les sorties `direct-calls.log`, `direct-flows.log` et `raw-branches.log` sont
vides après exécution réussie. Elles excluent donc une simple lacune d'index de
références pour ces trois cibles exactes. Elles ne prouvent pas qu'aucun chemin
indirect ne les atteint.

## Prochaine preuve

Conserver le modèle historique hors runtime et ne reprendre cette famille que
si un nouvel export ou une trace qualifiée fournit une adresse source
reproductible. Le prochain travail AC6 rentable reste les chaînes de scènes
et ressources déjà raccordées ; cette sous-piste est `manual-review`, pas
`verified` ni `needs-dynamic-evidence`.

# Correction — `StartWorkerQueue` est un callback CPU transporté par PM4

Date : 2026-08-19

Ce rapport supplante la conclusion de :

```text
commit c2820200bfff954f11690a420d5ec25c082fa82e
reports/AC6_DEMO_STARTWORKERQUEUE_IS_QUEUED_ON_THE_GPU_NOT_CALLED.md
```

## Formulation retirée

```text
StartWorkerQueue est une commande GPU et le GPU exécute ensuite 0x821C4A60.
```

## Formulation correcte

```text
D3D écrit 0x821C4A60 dans scratch4
D3D écrit son paramètre dans scratch5
D3D émet PM4_INTERRUPT avec un masque CPU
le command processor déclenche l'interruption graphique source 1
0x821B9710 recharge callback/paramètre depuis l'état D3D
le CPU appelle 0x821C4A60(paramètre)
```

Le GPU transporte donc une requête de callback vers le CPU. Il n'interprète pas
l'adresse comme du microcode ou comme une cible PowerPC à exécuter lui-même.

## Effet sur la frontière

La présence de l'adresse dans le command buffer reste une preuve utile : elle
identifie le callback attendu. En revanche, elle ne permet plus de conclure :

```text
ring immobile
→ callback impossible
```

sans examiner l'ordre exact suivant :

```text
publication du buffer primaire
→ exécution scratch4/scratch5
→ PM4_INTERRUPT
→ livraison source 1
→ callback CPU
→ signal du worker
```

Le bridge natif réduit actuellement le masque PM4 à CPU 2 et diffère la
livraison jusqu'au prochain tour externe. Le contrat de masque est corrigé dans
un type autonome sur `main`; le changement de timing reste volontairement hors
du chemin par défaut jusqu'à une comparaison A/B bornée.

## Autre correction de nomenclature

L'opcode `0x58` est `EVENT_WRITE_SHD`, pas `EVENT_WRITE_EXT`. Le second est
`0x5A`. Le marqueur `0x821ADD90` reste une instrumentation GPU facultative ; seul
son nom précédent était faux.

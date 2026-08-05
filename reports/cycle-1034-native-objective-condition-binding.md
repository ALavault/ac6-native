# Cycle 1034 — liaison native des conditions d’objectif

Date : 2026-08-06.

Cette slice ferme une primitive native manquante sans inventer la sémantique
retail de Mission 01.

## Contrat implémenté

`MissionObjectiveDatabase` accepte toujours le manifeste historique à quatre
colonnes. La forme qualifiée pour une condition liée à une unité est :

```text
mission_id  objective_id  stable_id  required  condition  target_entity
```

Les seules conditions natives sont `manual`, `destroy_unit` et
`protect_unit`. Une condition non manuelle doit nommer explicitement une
entité; un identifiant absent du `UnitRegistry` reste sans effet. La résolution
se fait après synchronisation du registre avec `CombatWorld`, et une transition
terminale rend la frame non jouable avant sa publication au renderer.

La sauvegarde passe en version 9. Les versions 1 à 8 restent lisibles et
restaurent leurs objectifs comme conditions manuelles, sans valeur implicite de
cible.

## Preuves natives

- `tests/product_runtime_tests.cpp` charge les deux formes de manifeste,
  rejette une cible nulle sans écraser la base déjà chargée, puis vérifie
  succès `DestroyUnit` et échec `ProtectUnit` sur l’entité 4098.
- `tests/session_save_tests.cpp` vérifie la persistance de la condition et de
  la cible dans un checkpoint version 9.
- Validation post-slice : 10 tests Python, compilation des cinq outils,
  compilation native `-j32`, puis 5/5 tests CTest sous SDL audio dummy et
  Xvfb. Le replay natif post-slice conserve le hash de session
  `c80bebca9624bceb407f4d5162684fa04392b197e09cd3bb82a5cdc7a0465f71` et
  `deterministic_replay=true`, `pause_stable=true`,
  `save_resume_stable=true`, `restart_stable=true`.
- `success_failure_debrief` reste ouvert : ces tests qualifient le mécanisme
  natif, pas encore l’identité retail des objectifs Mission 01.

## Expérience bridge bornée

Le binaire instrumenté a été recompilé et lancé avec audio dummy, profil
qualifié et cvars de télémétrie. Le run a produit 1 640 `PRESENT`, puis a
expiré avant `selector44=3`; aucun census d’unités ou de vtables gameplay n’a
été obtenu. Cette expérience est donc support uniquement et ne change aucun
gate natif.

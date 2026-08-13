# Cycle 1582 — M01-C verrouillage et tir déterministes

## Résultat

`RetailSession` possède maintenant une tranche de combat native bornée :

- X (`0x4000`) sélectionne au front montant l'ennemi hostile placé le plus
  proche, avec départage par `EntityId` ; les unités retail sans position ne
  sont pas transformées en cible à l'origine ;
- A (`0x1000`) déclenche le primaire `weapon_id=1` uniquement après cette
  action de verrouillage ;
- le projectile et les collisions utilisent `CombatWorld`, sans progression
  ou destruction synthétique ; `target_entity()` expose le verrou courant ;
- le front montant est conservé par session et réinitialisé lors d'un restore
  de checkpoint.

Le primaire reste un profil provisoire (damage 100, vitesse 2000, cooldown
0.25 s, portée bornée à 1e9 pour ne pas confondre la qualification de portée
avec le port retail). Cette tranche ne ferme donc pas encore la lane combat.

## Validation

```text
ac6-retail-session-tests sur le payload M01 PAL                         pass
  ticks=1800, steps=6, advances=6, ended_at=1800, completed=4
  contrôle positif : X -> target_entity != 0 ; A -> 1 projectile actif
cmake --build ... --target ac6-retail-session-tests -j16                  pass
```

Les assets PAL restent ceux de l'extraction scellée ; aucun conteneur retail,
tracker, tracking ou telemetry n'est ajouté.

## Reste à qualifier

Les paramètres avion/arme retail, trajectoire exacte, dégâts/destruction IA,
compteurs d'objectifs et le cône d'observation de la première cible doivent
encore être portés et confrontés au corpus oracle avant de promouvoir M01-C.

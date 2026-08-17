# Cycle 1599 — arme M01 produit fermée faute de WeaponBin qualifié

## Résultat

Le profil d'arme `{id=1, dégâts=100, vitesse=2000, cooldown=0,25,
portée=1e9}` n'est plus installé dans une session issue du cache retail. Ces
valeurs n'étaient reliées ni au loadout du frontend ni aux trois pointeurs
WeaponBin de l'ObjBin PAL ; elles constituaient donc une sémantique synthétique.

Le chemin store-backed conserve l'identité du loadout dans le bundle et le
frontend, mais lance provisoirement le runtime avec zéro définition d'arme.
Il expose `primary_weapon_id=0`, `weapon_count=0` et refuse `fire_primary()`.
Le tir déterministe demeure uniquement dans l'overload payload, explicitement
diagnostique, afin de tester le projectile et la collision sans en faire une
preuve produit.

`fire_primary()` n'utilise plus un identifiant global : il consomme désormais
l'identifiant primaire publié par `MissionExecution`, lorsqu'il existe.

## Preuve et frontière

Le schéma qualifié de `0x8232F198` établit trois PointerRecord WeaponBin aux
enfants 3, 4 et 5 de l'ObjBin de 0x20 octets. M01 remplit respectivement 152,
189 et 93 slots. Il ne qualifie toutefois ni les champs internes WeaponBin,
ni la relation entre le loadout et une définition combat. Aucune statistique
n'est donc inférée de ces seuls pointeurs.

## Contrôles

* build ciblé `ac6-retail-session-tests` : passé ;
* CTest `ac6-retail-session` avec payload PAL : 1/1 passé ;
* garde ajoutée : une session cache-backed n'expose aucune arme synthétique et
  refuse le tir ;
* `git diff --check` ciblé : passé.

La lane objectifs/campagne reste ouverte. Sa fermeture exige le lecteur borné
WeaponBin/DurableBin, le raccord loadout → arme, les producteurs cible et
destruction, puis les trois gardes scheduler retail.

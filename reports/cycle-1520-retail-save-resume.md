# Cycle 1520 — reprise retail liée au curseur de script

## Delivered

Les checkpoints de `RetailSession` enveloppent désormais l'état de
`MissionExecution` et le curseur du script retail (sous-mission, étape et code
de fin). Le format de sauvegarde atomique passe en v11 ; les versions v1 à v10
restent lisibles pour migration, mais une reprise retail exige le marqueur de
curseur v11 afin de ne jamais redémarrer silencieusement au premier step.

`ac6-native play --resume SAVE_PATH` charge le slot 1, vérifie l'identité
SHA-256 du `RetailContentStore` et restaure les deux états ensemble. Une
sauvegarde d'un autre cache, d'une autre mission ou sans état de script est
refusée avec `save_incompatible`. Les tests couvrent le round-trip v11, le
rejet d'un marqueur incohérent et la poursuite déterministe d'une session
payload-only après restauration.

## Validation

- Build ciblé `ac6-session-save-tests`, `ac6-retail-session-tests` et
  `ac6-native` : passe.
- Reprise manuelle sur le cache
  `/tmp/ac6-retail-v2-smoke` (index
  `cfca517e3f843169ca01fc52700472e66b86365621a922fc27a64a21ab713f85`) : les
  missions 1–14 terminent leur script dans la sonde de 8 ticks ; la mission 15
  reste en gameplay comme attendu ; la session longue termine à 1800 ticks.

## Boundary retained

Les compteurs et horodatages du `SubMissionSequencer` retail ne sont pas encore
inclus dans le checkpoint générique ; les conditions tag-7 et les producteurs
IA/combat restent donc une frontière à fermer avant une reprise de campagne
complète. La migration de format est volontairement conservatrice : les
anciennes sauvegardes sont lisibles pour inspection mais non rechargeables dans
une session retail sans curseur qualifié.

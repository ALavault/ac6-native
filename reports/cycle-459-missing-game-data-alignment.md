# AC6 cycle 459 — alignement du dialogue de données absent

Date : 2026-08-01  
Cible : AC6 PAL, base image `0x82000000`, XEX qualifié par le handoff.

## Résultat

Le texte natif de secours du dialogue `M70000_122` est maintenant centré dans
le corps du panneau, au-dessus des boutons `YES/NO`. Le défaut était limité à
la couche de présentation native : le panneau et les boutons invités étaient
corrects, mais le fallback commençait à `y=438`, sous la rangée des boutons.

L'oracle 1280×720 place les deux lignes à `y=278` et `y=312` dans le viewport
de jeu. Le layout natif utilise désormais ces coordonnées de référence, un
interligne de 34 pixels et une mise à l'échelle proportionnelle à la hauteur du
viewport. Le centrage horizontal reste calculé depuis l'étendue réelle de
chaque ligne.

## Validation

- Test unitaire du texte et du layout : inclus dans **6/6 tests AC6 pass**.
- Run natif borné, action
  `2a380aa292ac237b2ec7727d653bfab677697580b7fbd074efe8838855a82084` :
  `type28=30` atteint, 30 présentations de stabilisation, capture réussie.
- Capture native 1280×720 :
  `reports/logs/cycle-459-dialog-alignment/step-02-missing-game-data.png`,
  SHA-256 brut
  `f24e651505c95d0971ee4fb7ab5fb3bea490fcd5ca67be5f47133ad977871716`.
- Référence oracle : `cycle-454-clean-profile-oracle/t52-second.png`, SHA-256
  `f7716940466c3141420b150a2d229932ff96f189ffa2f174c6bbff6e08e85e96`.
- CTest complet : **1609 pass, 4 skip, 6 échecs de référence sur 1619** ;
  aucune régression par rapport au cycle 458.
- Binaire final SHA-256 brut :
  `f7e4235654b2987abc7c8d6058e6820fbf5a9f562d360eebf105807559c6a806`.
- `git diff --check` passe ; aucun processus `ac6recomp` ou Xvfb orphelin.

JUnit : `reports/logs/cycle-459-dialog-alignment-full-ctest.xml`.

## Portée restante

Cette correction ne modifie ni le flux de sauvegarde ni la transition
campagne. L'écran noir vivant post-cutscene reste la frontière de jouabilité.
Le dialogue `PLEASE WAIT` montrant l'atlas de police et le mismatch global de
résolution restent des défauts adjacents distincts.

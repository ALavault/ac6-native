# Cycle 372 — dispositions hôte et invitée : accord parfait, y compris pour les fautives

## 1. Mesure

| base | taille | host_x_pitch | guest_x_ext | host_y_pitch | guest_y_ext | rend |
|---|---|---:|---:|---:|---:|---|
| 028B2000 | 64x64 | 16 | 16 | 16 | 16 | oui |
| 028D0000 | 64x720 | 16 | 16 | 180 | 180 | oui |
| 028E9000 | 960x264 | 240 | 240 | 66 | 66 | oui |
| 0294A000 | 208x48 | 52 | 52 | 12 | 12 | oui |
| 02953000 | 224x64 | 56 | 56 | 16 | 16 | oui |
| **028B7000** | 320x180 | **80** | **80** | **45** | **45** | **non** |
| **03514000** | 256x256 | **64** | **64** | **64** | **64** | **non** |

**Les extensions hôte et invitée coïncident exactement pour les sept.** Aucune
troncature, aucun décalage — y compris pour la texture à hauteur impaire en
blocs (`45`) et pour celle sans rembourrage.

## 2. Ce que cela élimine

La dernière hypothèse issue du cycle 371 — un rembourrage nul ou une hauteur
impaire se traduisant en copie tronquée côté hôte — est **réfutée**. Les deux
singularités de disposition existent bien, mais elles **n'engendrent aucune
divergence hôte/invité**.

Dix-neuf causes éliminées.

## 3. L'état, sans enjolivure

Pour ces deux textures, **toute propriété mesurée est identique** aux cinq qui
s'affichent : format, constante de fetch complète, vue d'image, chargement,
invalidation, données sources, load shader, pavage, disposition invitée
calculée, disposition hôte. Et pourtant l'échantillon est nul.

Une contradiction aussi complète signifie que la cause est dans quelque chose
**qui n'a pas encore été observé**, non dans une différence de paramètre. Deux
candidats, aucun mesuré :

1. **l'exécution de la copie** — le *load shader* est choisi et le pipeline
   existe, mais rien ne prouve que la dispatch s'exécute ni qu'elle écrit dans
   l'image pour ces deux textures ;
2. **l'occlusion** — les quads de glyphes seraient rendus puis recouverts par un
   dessin ultérieur de la même passe. Le test du cycle 360 forçait *tous* les
   échantillons de la passe au blanc, donc il ne pouvait pas révéler un
   recouvrement interne.

Le second est neuf et n'a jamais été envisagé : il expliquerait un échantillon
correct et un résultat invisible, ce qu'aucune mesure de texture ne peut
distinguer.

## 4. Front suivant

Départager par ordre de dessin : journaliser, pour la passe, l'ordre des dessins
et la texture liée à chacun, puis vérifier si un quad postérieur recouvre la zone
des lots de glyphes. C'est une mesure de géométrie, pas de texture — un domaine
resté inexploré depuis le cycle 349.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

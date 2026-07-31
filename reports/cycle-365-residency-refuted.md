# Cycle 365 — la résidence est réfutée : les deux planches sont bien chargées

## 1. Mesure

Sonde de chargement du cycle 352 étendue pour journaliser **sans
échantillonnage** toute charge des deux planches de glyphes du cycle 362 :

```
load #2310 base=028B7000 format=20 320x180   <-- GLYPH SHEET
load #2321 base=03514000 format=20 256x256   <-- GLYPH SHEET
```

**Les deux sont chargées.** L'hypothèse « résidence » du cycle 364 est réfutée.

L'extension était nécessaire : l'échantillonnage d'origine (8 premières charges
puis une sur 200) les aurait manquées, et leur absence aurait été lue à tort
comme « jamais chargées ». Le cycle 350 avait déjà signalé ce biais.

## 2. État de la contradiction

Pour ces deux textures, tout est mesuré sain :

| propriété | état |
|---|---|
| chargée dans le cache | **oui** (ici) |
| vue d'image réelle liée | **oui** (cycle 364) |
| descripteur identique aux textures qui marchent | **oui** (cycle 363) |
| format identique (`fmt=20`) | oui (cycle 362) |
| passe qui les dessine peint l'écran | **oui** (cycle 360) |
| géométrie, viewport, cible de rendu | sains (348, 349) |
| **échantillon** | **zéro** |

Treize causes éliminées. La contradiction est complète : rien de mesurable ne
distingue ces deux textures des cinq qui s'affichent, et pourtant elles ne
rendent rien.

## 3. Ce qui reste, et c'est court

1. **Invalidation** — l'invité écrit les données *après* le téléversement et le
   cache ne le voit pas. La texture est alors chargée, liée, décrite
   correctement, et vide. C'est la seule cause restante cohérente avec
   *l'ensemble* du tableau.
2. **Coordonnées de texture** `r0.xy` de la passe — jamais vérifiées.

Le point 1 explique tout sans exception ; le point 2 reste possible mais
expliquerait mal que cinq textures de la même passe s'affichent.

## 4. Front suivant

Journaliser, pour ces deux bases, l'horodatage de chargement contre les
écritures invitées dans la même plage — le cache expose une invalidation par
plage mémoire. Si l'écriture suit la charge, la cause est établie.

À défaut, forcer les coordonnées `r0.xy` à une constante dans la passe, comme
l'a été la couleur de sommet au cycle 356.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

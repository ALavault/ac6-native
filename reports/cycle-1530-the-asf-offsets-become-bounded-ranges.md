# Cycle 1530 — les offsets ASF deviennent des plages bornées

## Résultat

`RetailAsfIndex` conserve maintenant les 5 449 offsets retail des deux banques
`moviepack.bin` et expose chaque intervalle adjacent comme une plage absolue
bornée dans le blob content-addressed. Le dernier intervalle se termine à la
fin de sa banque. Ces intervalles restent volontairement nommés « plages
indexées » : aucune sémantique packet/frame, aucun codec et aucun timestamp ne
sont inventés.

La lane XMA/ASF reste ouverte. Ce lot prépare une AVIO bornée mais ne rend pas
les fragments individuellement décodables et ne produit aucune synchronisation
audio/vidéo.

## Invariants et contrôles

Le parser exige déjà le préfixe BNK, les GUID ASF File Properties/Header
Extension, les tailles de banque et une table d'offsets strictement croissante.
Il scelle désormais tous les mots de cette table et refuse en plus un premier
offset qui chevauche les métadonnées et le trailer.

Le contrôle synthétique vérifie trois offsets `200,300,400`, les plages
`[200,300)`, `[400,512)`, les indices hors limites, l'offset absolu de la
seconde banque et le rejet d'un premier offset situé dans les métadonnées.

Sur le cache PAL qualifié :

```text
banque 0 : offset 0,         taille 164638720, 2630 plages
  première  [28142156,28193956)  taille 51800
  dernière  [164634736,164638720) taille 3984
banque 1 : offset 164638720, taille 183668736, 2819 plages
  première  [193514776,193533344) taille 18568
  dernière  [348303760,348307456) taille 3696
total : 5449 plages, toutes non vides et contenues dans leur banque
```

Le même exécutable peut sélectionner `AC6_ASF_CACHE` pour ce contrôle sans
déclencher le décodage XMA complet; `AC6_MEDIA_CACHE` conserve sa route
qualifiée historique qui exécute les deux contrôles.

## Validation

```text
build complet                                           pass
CTest, cache retail + cache ASF + SDL dummy + Xvfb      81/81, skips 0
test retail ASF qualifié                                pass, 2 banques/5449 plages
contrôle synthétique/troncature/overlap                 pass
git diff --check                                        pass
```

## Frontières restantes

Il faut encore qualifier la structure interne d'une plage, fournir un demux
FFmpeg strictement borné, sélectionner voix EN/JP et sous-titres PAL, puis
sceller timestamps, synchronisation A/V et événements temporels. Le fait qu'une
plage soit bornée ne prouve pas qu'elle constitue une unité de décodage.

# Cycle 1531 — le corpus Scene/TCAM entre dans le bundle natif

## Résultat

`RetailCampaignBundle` construit maintenant un catalogue Scene/TCAM manuscrit
directement depuis chaque payload campagne importé. Le lecteur descend les FHM
bornés, exige les triplets frères `NFICCUT` / ressources FHM / `Scen`, joint
chaque chemin au slot ressource correspondant et publie uniquement les
`Tcam*.mop` dont le wrapper MOP et la structure GYZ sont valides.

Le catalogue conserve le chemin retail, la route d'indices FHM, l'offset dans
le payload, la taille et le SHA-256. Les octets restent possédés par le bundle :
aucun pointeur vers une PAC ou le cache ne survit à l'import et les vues TCAM
restent valides après déplacement du bundle.

## Bornes et rejets

Le scan est tout ou rien. Il borne la profondeur à 64 FHM et le nombre de
conteneurs à 100 000, refuse les extents vivants qui se chevauchent ou sortent
du payload, les tables `Scen` qui ne sont pas un multiple exact de `0x80`, les
chemins sans terminaison ou padding nul, les cardinalités divergentes, les
ressources vides, les chemins TCAM dupliqués et les offsets GYZ hors bornes.

Les trois records GYZ de `0x30` octets restent opaques. Seuls leurs extents et
leurs deux offsets de données déjà prouvés sont exposés ; aucune interpolation,
orientation, durée ou activation n'est inventée.

## Contrôle PAL quinze missions

Sur le cache v2 qualifié, le test natif retrouve exactement la matrice scellée
par l'audit Python :

```text
missions       15
tables Scen    176
chemins        2950
ressources TCAM 88
```

Pour M01, il retrouve
`Scene/dd01_01a/dd01_01a_01/Tcam__c01.mop`, taille 4 656, SHA-256
`2af69c5ebdf322c473b7aa4599882dc2d5b915a4433a9e58f0ea6ea3340cf2d1`
et route FHM `22/1/0/1/0` depuis le payload de l'entrée 9.

## Validation

```text
build ac6-retail-scene-tcam-tests                     pass
exécutable, AC6_RETAIL_CACHE qualifié                 15/176/2950/88
CTest ac6-retail-scene-tcam, cache qualifié           1/1
contrôles synthétiques et rejets                      pass
git diff --check                                      pass
```

## Frontières restantes

La lane Scene/TCAM reste ouverte. Le catalogue ne sélectionne pas encore les
groupes à l'exécution et ne porte ni records/interpolation, ni activation NFIC,
ni producteurs live de pose/FOV, clipping, culling ou transitions caméra.

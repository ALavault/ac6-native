# Cycle 1528 — les quinze payloads Scene/TCAM sont recensés

## Résultat

Le corpus Scene/TCAM campagne est maintenant scellé sur les entrées DATA.TBL
`9..23`, soit M01 à M15 : 176 tables `Scen`, 2 950 chemins et 88 ressources
`Tcam*.mop`. Deux parcours indépendants du cache PAL v2 produisent le même JSON
octet pour octet, SHA-256
`1a20ca778f73f62dd6155788a566b591c4176beb1f1aa6a26a4450dfb2177ca9`.

La lane Scene/TCAM reste ouverte : cet inventaire qualifie les payloads et leurs
jointures, pas leur sélection runtime, l'interpolation des variantes, les poses
live, le FOV, le clipping/culling ou les transitions cinématiques.

## Corpus et invariants

Le cache qualifié porte l'index
`cfca517e3f843169ca01fc52700472e66b86365621a922fc27a64a21ab713f85`,
926 blobs et les quinze missions. Le parcours exclut explicitement les doublons
de scènes `103/109/111/115/117`, les mondes `119..133` et les scènes `ed*`
`475..533` : ils ne font pas partie des quinze payloads campagne.

```text
M01  44 /  553 / 22    branches 22,23
M02–M06, M08, M10–M12, M14
      0 /    0 /  0
M07  24 /  290 / 12    branches 22,23
M09  16 /  662 /  8    branches 22,23
M13  30 /  363 / 15    branches 22,23
M15  62 / 1082 / 31    branches 22,23,24,25
total 176 / 2950 / 88  (Scen / chemins / TCAM)
```

Chaque table `Scen` est une suite non vide de records de `0x80` octets : chemin
ASCII NUL-terminé sous `Scene/`, puis padding nul. Elle doit posséder les deux
frères exacts `N-2=NFICCUT` et `N-1=FHM ressources`; le nombre et l'ordre des
enfants ressources doivent égaler ceux des chemins. Chaque TCAM valide le
wrapper MOP, `GYZ\0`, le tag `0x00011E00`, trois records de stride `0x30` et
leurs deux offsets de données bornés.

Les slots FHM à taille déclarée nulle restent opaques et ne sont jamais
déréférencés, y compris lorsqu'ils réservent une position au milieu de la
table : cette forme retail mesurée possède un contrôle de non-régression.

## Artefact sans contenu retail

`reports/ac6-pal-scene-tcam-corpus.json` ne contient que les identifiants de
mission/entrée, tailles, SHA-256 et chemins de ressources. Son schéma fermé
refuse toute clé supplémentaire, tout chemin absolu, toute forme de SHA
invalide, tout ordre ou compte divergent et toute tentative d'ajouter un champ
payload. Aucun octet de blob ni chemin absolu du cache n'est sérialisé.

## Validation

```text
corpus retail, deux parcours                             15/15, byte-identiques
matrice metadata-only                                   pass, 176/2950/88
tests Python                                             164/164
ruff tools scripts                                      pass
build complet                                           pass
CTest, cache retail + SDL dummy + Xvfb                  81/81, skips 0
Mission 01 JF                                           pass
checkpoint 2                                            open, 0/6 lanes
git diff --check                                        pass
```

## Frontières restantes

Le décodage sémantique des trois records TCAM, leur interpolation et leur
activation par les événements NFIC restent à relier au runtime. Les producteurs
caméra live, le chemin VMX général, le FOV, le clipping/culling et les
transitions doivent encore être dérivés et testés avant de fermer Scene/TCAM.

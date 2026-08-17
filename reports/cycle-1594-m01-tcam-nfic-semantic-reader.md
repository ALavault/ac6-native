# Cycle 1594 — lecteur sémantique TCAM/NFIC M01

## Résultat

La jointure sérialisée de la première caméra M01 est maintenant consommée par
le produit natif, avec contrôles positifs et rejets bornés :

* `RetailTcamCameraTrack` valide les trois records GYZ contigus ;
* le record 0 fournit 121 clés de position, le record 1 121 clés angulaires,
  le record 2 un FOV vertical ;
* les temps big-endian sont strictement croissants et, dans M01, couvrent
  exactement `0..120` ;
* l’échantillon demandé par le premier événement CUT, clé 1, produit
  `[-15824.801758, 3284.871826, -1023.996643]`, l’orientation
  `[-1.284656, 0.284061, 0.000019, 0]` à la tolérance du contrôle, et un FOV
  d’environ `0.761` radian ;
* `RetailNficCutView` valide les neuf chunks, le dictionnaire `0x3041`, le
  flux TLV `0x3040`, son terminateur nul et son `CutTerminate` final ;
* `RetailCampaignBundle::nfic_cut_bytes` conserve la jointure exacte du frère
  `NFICCUT` avec chaque ressource TCAM, y compris après déplacement du bundle.

L’orientation reste une suite de valeurs angulaires sérialisées. Aucun ordre
Euler retail ni matrice de caméra n’est promu par ce lecteur.

## Contrôle M01 qualifié

Cache : PAL v2, index
`cfca517e3f843169ca01fc52700472e66b86365621a922fc27a64a21ab713f85`.

Ressource :
`Scene/dd01_01a/dd01_01a_01/Tcam__c01.mop`, 4 656 octets,
SHA-256 `2af69c5ebdf322c473b7aa4599882dc2d5b915a4433a9e58f0ea6ea3340cf2d1`.

État adjacent : 39 352 octets, neuf chunks et 2 402 événements hors
terminateur. Le début vérifié est :

```text
0x8001 CutStart         payload 0
0x8002 FrameStart       payload BE u32 1
0x1001 MoveCamera       payload 0001000000010000
```

La commande caméra sélectionne l’objet Scene one-based 1 et la clé TCAM 1.
Cette sélection est conditionnelle au groupe Scene résolu ; elle ne prouve
pas que l’état campagne active ce groupe dans le runtime retail.

## Rejets

Les tests courants rejettent :

* compte GYZ incohérent et offsets non contigus ;
* valeurs flottantes non finies, FOV nul et temps non monotones ;
* chunk NFIC surdimensionné ou réservé non nul ;
* flux événementiel sans terminateur, sans `CutTerminate` final ou avec
  dictionnaire incomplet.

## Validation

```text
cmake --build reconstruction/ace-combat-6/build -j16 --target ac6-retail-scene-tcam-tests  pass
CTest ac6-retail-scene-tcam avec cache PAL                             1/1 pass
audit_cpp_complexity                                                     pass, 284 fichiers
audit_ac6_product_boundary                                               pass
audit_ac6_global_ladder                                                   pass
git diff --check                                                          pass
```

La lane Scene/TCAM reste ouverte : activation NFIC runtime, sélection de
groupe par la campagne, pose live, clipping/culling, transitions, unités,
matériaux et rendu Xenos complet ne sont pas fermés par cette tranche.

# Cycle 1630 — premier consumer guest de START

Date : 2026-08-15  
Cible : démo Xbox LIVE PAL `Default.xex`  
SHA-256 : `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`

## Résultat

Le premier consumer guest du bit START est maintenant identifié sur preuves
PAL, sans hook synthétique ni cross-match retail.

Au tick 252, l'appel XAM qualifié au LR `0x822F616C` reçoit :

- objet input `r31 = 0x829D153C` ;
- buffer `XINPUT_STATE` `r5 = 0x829D1580`, soit objet `+0x44` ;
- `state16 = 00000001001000000000000000000000` ;
- résultat `r3 = 0`.

La fonction `.pdata` `0x822F6108..0x822F619B`, SHA-256 bytes
`43aa4f1f…7586`, teste ce résultat puis appelle immédiatement
`0x822F6008` au LR `0x822F617C`.

Dans `0x822F6008..0x822F607F`, SHA-256 bytes `fd053655…cff9`, la première
lecture est :

| PC | bytes PAL | instruction | effet |
|---|---|---|---|
| `0x822F601C` | `A1 7F 00 48` | `lhz r11,0x48(r31)` | lit les boutons du `XINPUT_STATE` |
| `0x822F6020` | `91 7F 00 1C` | `stw r11,0x1C(r31)` | publie les boutons courants |

L'offset `0x48` est exactement objet `+0x44` (début état) `+4` (boutons
16-bit après le packet number). START est donc consommé par le guest avant
toute divergence scheduler ou renderer.

## Propagation littérale

La même fonction transforme ensuite l'état, sans sémantique inventée :

- ancien masque : objet `+0x74` ;
- masque courant : objet `+0x1C` ;
- objet `+0x14 = (ancien XOR courant) AND courant` ;
- objet `+0x18 = (ancien XOR courant) AND NOT(courant)` ;
- objet `+0x14` est donc le candidat « nouveau press » et `+0x18` le candidat
  « release », noms seulement descriptifs de la formule ;
- `0x822F5E58` reçoit `r4 = (ancien != courant)` et peut publier le masque
  courant à objet `+0x24` selon ses compteurs temporels ;
- objet `+0x74` est enfin remplacé par le masque courant à `0x822F6068`.

Le prochain appel XAM, tick 253 / LR `0x822F60A8`, contient boutons zéro et
packet number 2. Le masque START est donc une impulsion d'un tick ; aucune
preuve ne montre encore quel consumer aval lit objet `+0x14` avant son
remplacement.

## Provenance

- bytes directs : `xex-basefile.bin` SHA-256
  `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` ;
- atlas statique canonique : frontières `.pdata`, hashes bytes et pseudocode ;
- XenonRecomp : contrôle de flux et ABI littéraux seulement ;
- movie XAM START : caller, registres, tick et 16 octets exacts.

Qualification : `demo-qualified` pour le consumer et les formules ; rôle
fonctionnel aval encore `unknown`. Aucun nom retail, actif propriétaire ou
sortie générée n'est suivi.

## Prochain checkpoint

Tracer les lectures guest de l'adresse dynamique objet `0x829D1550`
(`+0x14`) entre `0x822F6054` et la fin du tick 252, en enregistrant seulement
PC/LR/thread/tick et valeur. Le premier reader doit ensuite être recoupé avec
ses bytes PAL avant toute nouvelle injection START.

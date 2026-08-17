# Cycle 1723 — fermeture structurelle PM4 des registres opaques atteints

## Verdict

Une exécution codegen-ON fraîche neutral, puis une exécution codegen-ON
fraîche avec `START` au tick 252, consomment toutes deux les mêmes deux IB
PAL sans trap PM4 : l’IB intermédiaire contient 11 dwords et l’IB principal
3 029 dwords / 871 packets. Le packet type-0 à l’offset 2 écrit les quatre
valeurs exactes observées à `0x0A02..0x0A05`. Elles sont admises uniquement
comme stockage transactionnel opaque; leur nom et leur effet matériel restent
inconnus. Toute autre valeur à ces indices est toujours rejetée avant commit.

Le parseur atteint 5 chargements de shaders par hash, 26 draws et un
`XE_SWAP` vers la ressource tiled 1280×720 à `0x1374A000`. Les résultats
neutral/START sont identiques pour le ring, les IB, les commandes typées et les
effets PM4; les traces globales diffèrent comme attendu à cause de l’entrée
START. Cela ferme la frontière structurelle et transactionnelle du batch
atteint, pas le contenu EDRAM, les pixels, le menu ou la mission.

## Identité, outil et preuves

| élément | valeur |
|---|---|
| cible | `Default.xex`, démo PAL, Xenon BE/Xenos |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| binaire codegen-ON | SHA `a4a5b57f2aaf7404fe2a84f8f7ef34a974b1304c14f81de8021271d183957811` |
| parser | `src/xenos_command_processor.cpp`, SHA `7a52ca99de7058e08282d8e97b3dd454295dd51d720925441da11f86281507a8` |
| inventaire neutral | `analysis/demo/ac6-demo-neutral-pm4-inventory-v2-opaque-registers.json`, SHA `cd3a37a299fe48805fd80cc1804db99820d85bcd60265668b5ab8e13d2d5e868` |
| inventaire START | `analysis/demo/ac6-demo-start-pm4-inventory-v2-opaque-registers.json`, SHA `018d61e85e3f87d518c2289c65496b9de917dbd9abbe5890a1985f21926e21e0` |
| capsule | `analysis/demo/ac6-demo-pm4-opaque-registers-v2.json` |

Les rapports runtime sources sont conservés par hash seulement : neutral
`d96a9b68…b6a79`, START `0f66d089…d564e`; les traces sont
`1d41d2e2…7ebef7` et `31553733…360b2`. Aucun dword de microcode n’est copié
dans le rapport ou la capsule.

## Packet opaque et invariant transactionnel

| offset | header | indices | valeurs | statut |
|---:|---|---|---|---|
| 2 | `0x00030A02` | `0x0A02..0x0A05` | `C0100000, 07F00000, C0000000, 00100000` | stockage opaque qualifié |

Le packet et son payload ont respectivement les SHA
`e4356ed3…47312` et `dc30c314…62942`. Le tableau Xenos générique n’est pas
utilisé pour donner un nom AC6 à ces quatre indices; il justifie seulement la
possibilité d’un stockage de registre sans effet spécial dans cette étape.

## Commandes atteintes

- ring `0x126CA000`, capacité 131 072 dwords, deux soumissions;
- IB `0x127CA0C0/11`, SHA `ef7ab6e4…d2b0`;
- IB `0x1274A000/3029`, SHA `d121c8d8…358d6`, types `338/252/281`;
- cinq shader loads (hashes uniquement), 26 draws, un present;
- effets PM4 : 6 scratch writebacks, 4 RMW, 10 waits, 256 conditional writes,
  4 shader-done events, 2 ME_INIT et 10 écritures mémoire;
- frontbuffer observé : adresse `0x1374A000`, format 6, tiled, 1280×720.

Ces faits ne prouvent pas le contenu des tiles EDRAM ni un pixel non noir. Le
readback guest-owned et le frontend restent explicitement non qualifiés.

## A/B et prochain checkpoint

Neutral et START atteignent chacun 253 ticks / 116 PRESENT et le même
frontier scheduler. Leurs graphiques, IBs, commandes typées et effets sont
byte-identiques; seules les traces globales et quelques registres de contrôle
liés à l’entrée divergent. Le prochain checkpoint est donc le readback
guest-owned du resolve déjà borné, sans rouvrir la sémantique des quatre
registres opaques. L’audio XMA reste séparément bloqué au store
`0x7FEA1A80`.

## Classification et garde

- **demo-qualified** : identité, parsing exact, consommation transactionnelle,
  valeurs opaques exactes, draws/present et métadonnées de ressource;
- **demo-observed** : deux probes fraîches neutral/START;
- **xenia-generic** : modèle de registre générique uniquement;
- **unknown** : effets hardware opaques, EDRAM/pixels, frontend, mission et
  XMA.

La garde fail-closed est conservée; aucun fallback visuel, mapping retail,
modification Xenia/Ghidra/C++ généré ou actif propriétaire n’est ajouté.

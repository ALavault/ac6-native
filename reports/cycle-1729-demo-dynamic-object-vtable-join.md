# Cycle 1729 — jointure dynamique objet/vtable du frontier démo

## Verdict

Une sonde read-only, opt-in et désactivée par défaut joint maintenant le
callsite guest `LR=0x822E559C` à l’objet `0x82934280`, à la vtable
`0x8202A488` et à son slot 4 `0x822F8848`. La jointure est observée 853 fois
sur chacune des routes neutral et START, avec les mêmes valeurs et le même
SHA de stderr. Elle est donc `demo-qualified` comme frontière d’appel et de
données, mais ne donne pas encore le rôle sémantique du corps appelé.

## Identité et méthode

| élément | valeur |
|---|---|
| cible | `Default.xex`, démo PAL, Xenon big-endian/Xenos |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| basefile PAL SHA-256 | `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` |
| binaire de sonde SHA-256 | `43677602c888cf481e0d0498a19a99cdda201526d306a8e76c860a6962e164d0` |
| sonde | `AC6_DEMO_WATCH_INDIRECT_OBJECT=1`, `max_ticks=1100` |
| routes | stores neutral/start séparés; START injecté au tick 252 |
| moteur Xenia/ptrace | non utilisé pour ce cycle |

Le hook lit uniquement `r3`, le mot de vtable et le slot 4 lorsque les plages
sont mappées; il n'écrit ni état guest ni état renderer.

## Preuve dynamique

| champ | neutral | START |
|---|---:|---:|
| observations | 853 | 853 |
| ticks distincts | 851 | 851 |
| intervalle | 0–1099 | 0–1099 |
| objet | `0x82934280` | `0x82934280` |
| vtable | `0x8202A488` | `0x8202A488` |
| slot 4 | `0x822F8848` | `0x822F8848` |
| arguments r4/r5/r6/r7 | `0/0x57C/0x100/0` | `0/0x57C/0x100/0` |

Les deux stderr sont byte-identiques (`f775d146…1b2c5a`). Les rapports restent
`max_ticks=1100`, 23 threads bloqués, un `PRESENT`, sans frontend, mission ou
terminal. Le PM4 reste identique: IB intermédiaire
`ef7ab6e4…d2b0`, IB principal `d121c8d8…358d6`, 5 loads, 26 draws, 1 present.

## Recoupement statique

L’atlas RTTI PAL publie la vtable `0x8202A488` et le slot 4 vers
`0x822F8848`. L’atlas de décompilation fixe pour cette entrée une taille
`0xA8`, le hash de corps
`88bc7c97…84d1bdd6`, le hash de pseudocode
`a07d0310…e0e1a12` et les appels directs
`0x822E53B8`, `0x822E53F0`, `0x822F02F8`, `0x822F0410`, `0x822F6708`.
Ces noms sont des identifiants d’adresse qualifiés, pas une sémantique
inventée.

## Classification et limite

- **demo-qualified** : objet/vtable/slot/callsite, stabilité A/B, valeurs de
  registres observées et recoupement RTTI/basefile PAL.
- **demo-observed** : répétition jusqu’au tick 1099 et état scheduler/PM4.
- **xenia-generic** : aucune preuve nouvelle.
- **unknown** : rôle de `0x822F8848`, cause de la ré-entrée, premier writer
  EDRAM non nul, pixels, audio XMA et résultat de mission.

## Prochain checkpoint

Capturer les transitions d’état produites par le corps `0x822F8848`, puis
qualifier le premier store EDRAM non nul avant le resolve. Le hook reste
désactivé par défaut; toute valeur ou commande inconnue doit trap avant effet.

Capsule durable : `analysis/demo/ac6-demo-dynamic-object-vtable-join-v1.json`.

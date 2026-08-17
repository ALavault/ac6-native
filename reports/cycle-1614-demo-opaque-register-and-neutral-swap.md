# Cycle 1614 — stockage Xenos opaque et swap neutral frais

## Résultat

Le packet démo PAL `e4356ed3…7312` écrivant `0x0A02..0x0A05` est maintenant
admis comme stockage opaque, uniquement pour les quatre valeurs capturées
`C0100000 07F00000 C0000000 00100000`. L'autorité Xenos épinglée traite les
packets type 0 comme des écritures du register file et ne donne aucun effet
spécial à ces indices. Toute autre valeur trappe avant commit ; aucun nom de
registre n'est promu.

Un run neutral frais, store neuf, XEX
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`, atteint
253 ticks sans entrée et produit 116 notifications de présentation. Son rapport
temporaire SHA-256 est `d96a9b68…b6a79`. Les deux IB sont exactement :

- `0x127CA0C0`, 11 dwords, SHA-256 `ef7ab6e4…d2b0` ;
- `0x1274A000`, 3029 dwords, SHA-256 `d121c8d8…358d6`.

Le swap neutral qualifie encore le frontbuffer `0x1374A000`, 1280×720,
format brut 6. Il ne qualifie toujours pas la source du resolve, le pitch,
l'endian ou le readback ; aucune screencap n'est publiée.

## Provenance guest de l'IB

L'instrumentation ciblée prouve au tick 0, thread 1, des writers de l'IB
principal dans les fonctions démo `0x821BAAD0`, `0x821BA930`, `0x821AE1E8` et
`0x821C1E18`. Cela ferme l'absence de producteur dynamique constatée au cycle
1612 sans utiliser de preuve retail.

## Nouveau frontier PM4

L'inventaire durable
`analysis/demo/ac6-demo-neutral-qualified-pm4-inventory-v1.json`, SHA-256
`2c2d9374…4d797`, consomme les 3029 dwords en 871 packets sans resynchronisation.
Le premier inconnu avance exactement de `0x0A02` à l'écriture type 0 du registre
`0x2290`, offset dword 81 du main IB. Le runtime demeure fail-closed pour ce
nouveau champ.

## Shaders et ressources

L'inventaire XEX redacted couvre 7 wrappers NSXR et 14 ShaderContainer. Le
pixel shader atteint a deux containers byte-identiques. Une extraction logique
temporaire des 121 ranges qualifiées par `DATA.TBL`, via l'outil PAC existant,
ne trouve aucune occurrence littérale des trois microcodes vertex. Les payloads
et sorties temporaires ne sont pas suivis. Cette absence oriente vers un
producteur guest/SDK de packets plutôt que vers un container copié littéralement
et ne constitue pas encore une identité de shader.

## Validation

- test C++ ciblé `ac6-demo-xenos-command-tests` : PASS ;
- valeur divergente sur `0x0A02` : trap transactionnel vérifié ;
- inventaire NSXR : génération fraîche 2/2 byte-identique ;
- inventaire PM4 : consommation exacte, premier inconnu `0x2290` ;
- `git diff --check` : PASS.

Prochain checkpoint : qualifier `0x2290` depuis l'autorité Xenos et les usages
atteints, puis avancer packet par packet jusqu'aux paramètres complets du
resolve/readback neutral.

# Cycle 1607 — atlas statique exhaustif de la démo

## Résultat

La cible reste exclusivement la démo PAL `Default.xex` de SHA-256
`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`,
Ghidra `ace-combat-6-demo` / `PowerPC:BE:64:Xenon`. Aucun fichier ou indice
retail n'a été utilisé ou modifié.

Le nouveau format `ac6-demo-static-decomp-atlas/v1` réconcilie exactement les
12 857 records stricts : 8 327 entrées `.pdata`, 4 431 chunks Ghidra et 99
entrées confirmées non recouvertes. Chaque frontière possède son hash et sa
provenance. Les 3 041 220 octets de `.text` sont classés fonction ou donnée.

Le dernier trou était `0x82375984..0x823767C3`, soit exactement 228 records
d'import appelables de 16 octets. Ghidra avait créé 181 fausses fonctions d'un
octet sur leurs descripteurs non décodables. La plage et les deux paddings
terminaux adjacents sont maintenant des données confirmées par hash ; aucune
fonction synthétique n'a été créée.

## Reproductibilité et sémantique

Deux imports frais isolés ont produit des manifests et journaux byte-identiques :

- manifest : `576fa31e02b1c899cdc997b8a6e252d6d7785656d13067a9d8a54aeb2810086c` ;
- journal : `b133247ff63117a24b0871661ada71198811b2c39e6292dcd1dd96a57ad90f78`.

Le nouvel exporteur read-only `ExportDemoStaticSemantics.java` a ensuite donné
deux sorties byte-identiques, SHA-256
`e723930a01e9ba573d0d899e16ebbd51f31c6c4553f53cf5ccafee5f50ba7962`.
L'atlas final enrichi est byte-identique sur les deux imports, SHA-256
`7ee1e677dfac287fdcd8d80b1c5f34575cbabf1c41ab79e70bd1581f87114e2d`.

Il contient 10 937 décompilations réussies, 3 échecs, 5 timeouts, 1 912 états
`unavailable` lorsque le corps Ghidra ne concorde pas exactement avec la
frontière indépendante, 10 945 noms non promus, 28 535 arêtes directes,
29 982 références globales, 1 990 références de chaînes hashées et 1 102
arêtes vers les 228 imports appelables qualifiés.

Le census RTTI brut retrouve exactement 772 descripteurs de type, 801
complete-object locators et 801 vtables, avec 11 019 slots, 2 385 bases et
6 631 appartenances fonction/vtable. Les 7 415 sites computed-call/jump sont
tous exportés et restent explicitement `unknown` tant qu'une cible n'est pas
prouvée. Les 1 920 statuts de décompilation non réussis restent eux aussi
explicites ; aucune borne Ghidra incompatible n'est promue.

## Validation et frontière

- build existant : pass ;
- CTest : 14/14 ;
- CTest codegen ON : 13/13 ;
- audit source et complexité : pass ;
- deux codegens propres : manifest `b6fb4890…c201` et arbre généré
  `4ce7acfa…f98b`, byte-identiques, 0 diagnostic frontière/instruction ;
- deux imports, deux exports sémantiques et deux atlas : byte-identiques ;
- couverture `.text` : 3 041 220 / 3 041 220 octets.

Le record/replay XAM borné au tick 253 est aussi requalifié depuis deux stores
neufs. Le movie contient l'unique appel `XamInputGetState` au tick 252 et son
START `0x10`; le replay strict atteint le même frontier scheduler. Les deux
RTPLY-v4 sont byte-identiques (`50776916…aa6c`) et le movie vaut
`6ff80573…e95`. Cela ne qualifie toujours aucun frontend.

Le gate atlas statique large est fermé : deux imports frais produisent atlas et
cartes de symboles byte-identiques, chaque octet `.text` est classé, toutes les
frontières strictes sont réconciliées et le codegen reste reproductible. Les
rôles/confiances non prouvés restent explicitement `unknown` et aucun nom auto
n'est écrit dans Ghidra. Les six lanes jouables restent ouvertes ; aucun
frontend, pixel, audio ou résultat de mission n'est revendiqué.

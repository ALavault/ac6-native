# Cycle 1610 — progression scheduler démo et cross-match rendu retail

## Résultat démo qualifié

Le correctif de fairness du scheduler cède immédiatement l'exécution au waiter
réveillé lors de `RtlLeaveCriticalSection`. Depuis un store démo neuf et avec
START `0x0010` au tick 252, la file guest à base `0x82386CC0` alterne désormais
son indice actif, le worker 9 s'exécute, le thread primaire quitte la boucle
initiale et les PRESENT passent de 115 à 129. Le frontier avance de 252 à 266.

Six entrées callable internes, toutes atteintes dynamiquement et bornées dans
des chunks Ghidra démo confirmés, ont été ajoutées au codegen strict :

- `0x820EABA0`, vtable `0x82007D0C`, slot 10 ;
- `0x820D0F58`, `0x820D0F68`, `0x820D0F78`, vtable `0x8200654C`, slots 3 à 5 ;
- `0x82323600` et `0x82323630`, tables de dispatch `0x8264CDC0` et
  `0x8264CDF8` ;
- `0x820D0D18`, vtable `0x820064D8`, slot 6.

Chaque record porte ses bytes, son SHA-256, ses terminaux PPC, son owner ou
chunk conteneur, son callsite/LR, son tick et son thread. Le dernier codegen
compte 12 864 fonctions, 142 records configurés, 52 unités C++, zéro diagnostic
de frontière et zéro instruction non supportée. Aucun comportement guest n'est
émulé par ces ajouts.

## Audit retail read-only

Source secondaire exclusivement : oracle retail NTSC-U/J `default.xex`,
SHA-256 `6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`,
checkout `.tools/ac6-recomp-oracle-dcd41b` au commit
`dcd41b7457fcac8242f8ef40de83d1719390d5af`. Ces adresses et noms ne sont pas
des preuves de la démo PAL et ne sont pas intégrés à son atlas.

Le code retail annoté expose une carte ABI D3D utile comme motif : SetRT
`0x821D95C8`, SetDepth `0x821D9D38`, SetTexture `0x821DD0A8`, SetStreamSource
`0x821DC538`, DrawIndexed `0x821DEF18`, Clear `0x821E2380`, Resolve
`0x821E2BB8`, vblank `0x821EEAC8`, EndFrame `0x821EFE78` et Swap
`0x821F05BC`. Le curseur de commandes retail est publié à `device+48`.

Une signature PM4 retail distinctive est construite avant cette publication :
`{0xC0022100, 0x1841, 0xFFFFF8FF, 0|0x100, 0x1930, 3|0}`. La recherche des six
dwords contigus et du triplet stable, dans les deux endiannesses, ne trouve
aucun match littéral dans la basefile démo. Cela invalide seulement l'hypothèse
d'une table constante identique ; il faut rechercher la construction PPC par
stores et la forme ABI, sans delta d'adresse global.

Les rapports PAL retail antérieurs donnent aussi un motif NSXR : signature
`0x4E535852`, count `u16be(container+0x0A)`, premier descripteur `+0x20`, stride
`u32be(entry+0x18)`. Le contrat ucode observé est le range guest brut
`virtualSize + physicalOffset`, de longueur `shader.size`, identifié par hash
XXH3-64 avant swap. Il pourra uniquement servir à faire une correspondance
exacte de bytes avec les containers démo.

## Prochain test

Continuer le replay fail-closed au-delà du tick 266. En parallèle, rechercher
dans l'atlas démo les producteurs par forme : suites de stores et publication
d'un curseur, appels SetRT/SetDepth/Resolve par ABI, et layout NSXR exact. Toute
correspondance devra être requalifiée sur le XEX démo `de917873…5da8` avant
d'orienter le décodeur PM4 ou le chargement shader.

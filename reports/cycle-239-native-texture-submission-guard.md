# AC6 cycle 239 — garde de soumission texturée native

Date : 2026-07-18

## Question

Le shell de scène natif prouve-t-il que les textures décodées sont réellement
acceptées par SDL et soumises aux polygones, ou seulement qu'un lien
MATE/NDXR/NTXR a été trouvé avant le rendu ?

## Correction

Les compteurs `presented_textured_objects` et
`presented_textured_polygons` étaient calculés avant l'appel SDL. De plus, le
résultat de `SDL_UpdateTexture` n'était pas vérifié. Une texture décodée mais
refusée par le backend pouvait donc être annoncée comme présentée.

Le renderer retourne désormais un `NativeDrawStats` fondé sur les appels
réellement réussis :

- `SDL_UpdateTexture` doit réussir ; sinon l'objet suit le fallback filaire ;
- un polygone n'est compté qu'après succès de `SDL_RenderGeometry` ;
- les nouveaux champs JSON sont `submitted_textured_objects` et
  `submitted_textured_polygons` ;
- le CRC32 RGBA de la surface capturée est calculé avant l'écriture BMP.

Les anciens compteurs restent utiles comme disponibilité amont. Les nouveaux
compteurs qualifient la frontière de soumission hôte.

## Corpus et résultat

Le cas qualifié est le groupe de scène `22.1.0`, frame CUT 120, chargé depuis
le sélecteur campagne 1 et les trois fichiers retail locaux qualifiés.

Deux exécutions normales indépendantes et l'exécutable installé donnent :

```text
submitted_textured_objects=2
submitted_textured_polygons=115
capture_rgba_crc32=505561482 (0x1e22418a)
BMP SHA-256=796678ba63a9e1bccf4102c67af8210511b524c056299823e2af664220edab27
```

La capture fait 960x540 en BMP 24 bits. Le CTest
`ac6-campaign-scene-shell-capture-frame` verrouille les deux cardinalités et
le CRC, avec `SDL_VIDEODRIVER=dummy`.

## Validation

```text
cmake -S reconstruction/ace-combat-6 -B .build/ace-combat-6
cmake --build .build/ace-combat-6 -j16
ctest --test-dir .build/ace-combat-6 --output-on-failure
cmake -S reconstruction/ace-combat-6 -B .build/ace-combat-6-asan \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'
cmake --build .build/ace-combat-6-asan -j16
SDL_VIDEODRIVER=dummy .build/ace-combat-6-asan/ac6-scene-shell \
  --campaign-selector 1 DATA.TBL DATA00.PAC DATA01.PAC \
  --capture-frame 120 frame.bmp
cmake --install .build/ace-combat-6 --prefix "$PWD"
test ! -e bin/bin
git diff --check
```

- corpus normal : **44/44** ;
- capture sanitizer ciblée : succès, mêmes 2 objets, 115 polygones et CRC ;
- corpus sanitizer complet : **non revendiqué**. La tentative a rencontré les
  plafonds historiques de 60 s sur le catalogue shader et 30 s sur deux
  shells de scène. Le processus a été arrêté avant de répéter ce timeout sur
  les autres scénarios ; aucun défaut ASan/UBSan n'a été émis dans la capture
  ciblée, qui termine normalement mais dépasse le plafond de 30 s ;
- exécutable installé : mêmes compteurs, CRC et SHA BMP ;
- `bin/bin` absent.

## Limites

Ce cycle qualifie la soumission native de deux textures décodées sur une scène
et une frame. Il ne prouve ni parité pixel Xenos, ni blending, ni équation
spéculaire, ni ordre retail des draws. En particulier, il ne ferme pas la
jointure entry-163 MATE→permutation : elle reste
`needs-dynamic-evidence` et exige toujours une capture retail ordonnée. Aucun
Xenia, VNC, GUI ou geste humain n'a été utilisé.

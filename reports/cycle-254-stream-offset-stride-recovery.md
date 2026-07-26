# AC6 cycle 254 — récupération offset/stride des streams

## Identité et méthode

- target : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- Ghidra headless, projet canonique, lecture seule et sans analyse ;
- aucune session Xenia, GUI, VNC ou intervention humaine.

## Layout persistant confirmé

Dans `SetStreamSource 0x821DD068`, les arguments sont
`r3/r4/r5/r6/r7/r8 = device/slot/buffer/offset/stride/dirty-mask`.

Pour un buffer non nul :

1. `buffer+0x18 + offset` forme l'adresse effective de fetch ;
2. `buffer+0x1C - offset` est stocké comme taille restante ;
3. `stride >> 2` est stocké comme octet par slot ;
4. le pointeur buffer est toujours publié dans la table de 16 slots.

Les adresses persistantes sont :

```text
buffer[slot]          = device + 0x30A4 + 4*slot
remaining_size[slot]  = device + 0x077C - 8*slot
encoded_stride[slot]  = device + 0x30E8 + slot
```

Preuves PPC décisives :

- `0x821DD094` charge `buffer+0x1C` ;
- `0x821DD09C` calcule `size-offset` ;
- `0x821DD0C4` publie cette valeur dans le second mot du record inversé ;
- `0x821DD150` calcule `stride>>2` ;
- `0x821DD15C` publie l'octet par slot ;
- `0x821DD154` publie le buffer.

Le snapshot récupère donc :

```text
offset = load_be32(buffer+0x1C) - load_be32(remaining_size[slot])
stride = load_u8(encoded_stride[slot]) << 2
```

Pour un buffer nul, offset et stride sont explicitement remis à zéro dans le
snapshot, sans relire les champs encodés potentiellement anciens.

Confiance : `confirmed` pour les écritures, le layout et les conversions 32
bits ; les noms XDK exacts restent `cross-match`.

## Implémentation

Le checkout AC6Recomp contient maintenant :

- des constantes nommées pour les cinq offsets device qualifiés ;
- `RecoverStreamOffset` et `RecoverStreamStride`, avec assertions compile-time
  pour strides `0x34/0x14` observés ;
- un snapshot draw-time qui peuple buffer, offset et stride pour les 16 slots ;
- une signature vertex-fetch qui consomme déjà ces trois valeurs.

Le bridge continue à sélectionner le dernier draw capturé et à utiliser le
frame-end comme fallback. Aucun compteur setter n'est artificiellement
incrémenté.

## Validation

- stream/RT/depth : **35/35** assertions Ghidra exactes ;
- vertex declaration : **38/38** assertions Ghidra exactes ;
- test synthétique bridge et helpers offset/stride : succès ;
- trois sources : Clang 21 C++23, `-Wall -Wextra -Werror`, succès ;
- artefacts :
  - `artifacts/ac6-cycle254-stream-layout-validation.log` ;
  - `artifacts/ac6-cycle254-source-tests.log`.

Ni `ac6recomp_config.toml` ni les sorties générées n'ont été modifiés.

## Limites et prochaine frontière

- aucun build complet AC6Recomp n'est revendiqué ;
- la lecture draw-time est validée statiquement et synthétiquement, pas encore
  par un oracle Xenia ;
- les signatures disposent désormais d'un layout vertex complet au niveau des
  pointeurs/offsets/strides ; la prochaine frontière autonome est la jointure
  MATE/technique/permutation/shader/draw.

La nouvelle archive AC6 annoncée reste absente du filesystem observé.

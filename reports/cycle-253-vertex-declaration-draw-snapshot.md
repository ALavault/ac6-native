# AC6 cycle 253 — vertex declaration et snapshot draw-time

## Identité et portée

- target : `ac6-xbox360-pal` ;
- module : `default.xex` ;
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- image base : `0x82000000` ;
- analyse : Ghidra headless, projet canonique, lecture seule et sans analyse ;
- aucune session Xenia, GUI, VNC ou intervention humaine.

## Bind confirmé

Le setter canonique est la feuille `0x821DE790` :

```text
821DE790  stw   r4,0x2E24(r3)
821DE794  ld    r11,0x10(r3)
821DE798  oris  r11,r11,0x8
821DE79C  std   r11,0x10(r3)
821DE7A0  blr
```

ABI : `r3=device`, `r4=vertex declaration`. Le champ est `device+0x2E24` et
le dirty bit est `0x0008_0000`. Six appels directs qualifiés chargent une
entrée de table de declarations dans `r4`, appellent ce setter, puis configurent
un ou deux streams via `0x821DD068`.

La sémantique est également confirmée par la provenance de l'objet :

- `0x821DE898` compte des éléments de 12 octets jusqu'au halfword sentinelle
  `0x00FF` ;
- `0x821DE7A8` stocke count à `+0x18`, max stream à `+0x1C`, deux bitmaps à
  `+0x20/+0x28`, puis copie les éléments à partir de `+0x34` avec stride
  `0x0C` ;
- plusieurs chemins créent cet objet, le conservent, le publient à
  `device+0x2E24`, puis lient les buffers de vertex.

Confiance : `confirmed` pour l'objet, l'ABI, le champ et le dirty bit ;
`cross-match` pour le nom XDK exact `SetVertexDeclaration`.

## Setter non universel

Un hook à `0x821DE790` serait insuffisant. De nombreux chemins AC6 inlinent
exactement le store et le dirty bit. Le chemin représentatif
`0x82138C40..0x82138C4C` publie une declaration puis appelle immédiatement le
draw indexé `0x821DEED8`.

Les resets sont eux aussi inlinés :

- reset device `0x821E6E50` : zéro à `+0x2E24`, puis dirty bit ;
- destruction owner `0x821D8FB4` : zéro, dirty bit, libération de la
  declaration et annulation de `owner+0x15C`.

La frontière configurée `0x821ED210` dans le pré-draw commun est
conditionnelle : `0x821ED208` peut aller directement à `0x821ED214`. Le load
commun réel est `0x821ED218: lwz r21,0x2E24(r30)`. Ni `0x821DE790` ni
`0x821ED218` ne figurent actuellement dans `[functions]`.

## Fast path compatible avec la configuration actuelle

Le checkout AC6Recomp rafraîchit désormais les pointeurs liés directement
depuis le device dans `SnapshotShadowState(base, device)` :

```text
vertex declaration : device+0x2E24
index buffer        : device+0x308C
stream buffer i     : device+0x30A4+4*i, i=0..15
```

Cette fonction est appelée par les trois hooks de draw déjà configurés. Elle
observe donc l'état effectivement commité après les setters physiques ou
inlinés, sans modifier `ac6recomp_config.toml` ni aucune sortie générée. Le
snapshot conserve les offsets/strides stream déjà connus ; cette passe ne les
invente pas lorsqu'ils n'ont pas encore été capturés.

Le bridge backend choisit maintenant le shadow state du dernier draw capturé,
au lieu d'ignorer `FrameCaptureSnapshot`. En l'absence de draw, il conserve le
shadow frame-end comme fallback. `RenderEventSignature` expose et hache
explicitement `vertex_declaration` et `index_buffer`, et son nombre de streams
provient du même snapshot cohérent.

Le `index_buffer` entre aussi maintenant dans la signature de ressources. Les
compteurs `set_vertex_decl_calls`, `set_index_buffer_calls` et
`set_stream_source_calls` ne sont pas artificiellement incrémentés : cette
voie mesure l'état au draw, pas le nombre de setters exécutés.

## Validation

- `VerifyVertexDeclarationBindContracts.java` : **38/38** assertions PPC
  exactes ;
- sources `d3d_hooks.cpp` et bridge : Clang 21 C++23,
  `-fsyntax-only -Wall -Wextra -Werror`, succès ;
- test synthétique du bridge : succès pour sélection du dernier draw et
  fallback sans draw ;
- aucun fichier généré et aucune configuration ReXGlue/XenonRecomp modifiés ;
- artefacts :
  - `artifacts/ac6-cycle253-vertex-decl-bind-validation.log` ;
  - `artifacts/ac6-cycle253-vertex-decl-callers.log` ;
  - `artifacts/ac6-cycle253-vertex-decl-resets.log` ;
  - `artifacts/ac6-cycle253-vertex-decl-draw-consumer.log` ;
  - `artifacts/ac6-cycle253-hook-sources-syntax.log` ;
  - `artifacts/ac6-cycle253-backend-capture-bridge-test.log`.

## Limites et prochaine frontière

> **Mise à jour cycle 254.** Les offsets et strides stream ont ensuite été
> récupérés depuis les champs persistants du device. Les deux limites
> correspondantes ci-dessous sont donc supersédées par le rapport cycle 254.

- la compilation syntaxique ne remplace pas un build AC6Recomp complet ;
- les offsets et strides stream ne sont pas récupérés par le snapshot device ;
- la télémétrie setter exacte demandera encore une régénération reproductible
  avec `0x821DD068`, `0x821DD20C` et éventuellement `0x821DE790` ;
- la prochaine passe autonome peut qualifier les champs offset/stride stream,
  puis reprendre la jointure material/shader/draw à partir des signatures
  draw-time désormais alimentées.

La nouvelle archive AC6 annoncée n'était toujours pas visible lors de cette
passe. Aucun run humain n'est requis pour la prochaine frontière statique.

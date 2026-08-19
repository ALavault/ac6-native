# AC6 démo PAL — pipeline de screencaps d'audit du renderer

Date : 2026-08-19  
Cible : démo Xbox 360 PAL `Default.xex`  
XEX SHA-256 : `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`

## Verdict

Le renderer peut désormais publier une capture PNG d'audit à partir du dernier
buffer dont la provenance est déjà fermée :

```text
resolve Vulkan qualifié
→ writeback tiled dans l'allocation guest 0x1374A000
→ relecture guest
→ untile RGBA8 1280×720
→ égalité SHA-256 avec la source résolue
→ PNG RGBA8 + sidecar JSON
```

La capture est donc prise **après** la validation du writeback et avant la
libération du buffer tiled éphémère. Elle n'est ni une copie de fenêtre SDL, ni
un `vkQueuePresentKHR`, ni une reconstruction opportuniste depuis une texture
hôte différente.

Cette passe ferme le transport d'image et l'auditabilité. Elle ne prétend pas
que le renderer produit déjà une image jouable : le front qualifié actuel reste
intégralement noir.

## 1. Activation

L'export est désactivé par défaut. Il s'active avec un répertoire existant :

```bash
mkdir -p /tmp/ac6-audit-caps
AC6_DEMO_AUDIT_SCREENCAP_DIR=/tmp/ac6-audit-caps \
  ./ac6-demo-recomp probe \
    --store <store> \
    --until frontend \
    --max-ticks 800 \
    --trace /tmp/ac6-audit.rtply \
    --report /tmp/ac6-audit.json \
    --backend vulkan
```

La première jointure `normal draw → copy draw → XE_SWAP` qualifiée produit :

```text
ac6-demo-pal-present-tXXXXXXXXXXXX-pXXXXXXXXXXXX-<rgba-prefix>.png
ac6-demo-pal-present-tXXXXXXXXXXXX-pXXXXXXXXXXXX-<rgba-prefix>.json
```

Les fichiers sont créés avec une sémantique exclusive. Une collision de nom
refuse l'écriture au lieu d'écraser une preuve antérieure.

## 2. Point de capture

Le point d'insertion est `commit_reached_guest_present`.

Avant l'export, le code exige :

```text
present_joined == true
adresse guest == 0x1374A000
largeur == 1280
hauteur == 720
extent tiled == 0x398000
octets tiled complets
writeback dans une allocation détenue par le guest
relecture guest complète
untile RGBA8
SHA-256 linéaire identique au resolve Vulkan
```

L'export ne peut donc pas promouvoir un buffer intermédiaire, une canary
Vulkan ou une adresse non détenue par la démo.

## 3. Format PNG

Le nouvel encodeur :

```text
include/ac6demo/audit_screencap.hpp
```

produit un PNG :

```text
bit depth     8
color type    6, RGBA
interlace     0
filter        0 par ligne
compression   DEFLATE stored blocks dans un flux zlib valide
```

Il n'ajoute aucune dépendance externe. Les chunks `IHDR`, `tEXt`, `IDAT` et
`IEND` possèdent des CRC vérifiés. Le flux zlib possède un Adler-32 vérifié.

Les métadonnées embarquées comprennent :

```text
Software
ac6.target
ac6.stage
ac6.tick
ac6.present_count
ac6.guest_address
ac6.rgba8_sha256
ac6.gameplay_claim=false
```

L'alpha original est préservé. Contrairement à un PPM, le fichier reste donc
une représentation lossless des quatre canaux du readback qualifié.

## 4. Sidecar JSON

Le sidecar utilise :

```text
schema = ac6-demo-renderer-audit-screencap/v1
```

Il enregistre :

- identité du XEX ;
- tick et compteur de présentations ;
- étape de provenance ;
- adresse guest ;
- dimensions et format ;
- SHA-256 du PNG ;
- SHA-256 RGBA8 canonique ;
- nombre de pixels RGB non nuls ;
- statistiques d'alpha ;
- statut `rgb_all_black` ;
- absence explicite de claim gameplay.

Le sidecar est écrit avant le PNG. Si la publication du PNG échoue, le sidecar
est supprimé. Un JSON orphelin n'est donc pas laissé comme certificat d'une
image inexistante.

## 5. Vérification indépendante

L'outil :

```text
tools/verify_audit_screencap.py
```

vérifie sans bibliothèque d'image :

1. signature PNG ;
2. bornes de chaque chunk ;
3. CRC de chaque chunk ;
4. profil RGBA8 non entrelacé ;
5. décompression zlib ;
6. filtre nul par ligne ;
7. dimensions ;
8. SHA-256 RGBA8 ;
9. statistiques pixel ;
10. cohérence du sidecar lorsqu'il existe.

Commande :

```bash
python3 tools/verify_audit_screencap.py \
  /tmp/ac6-audit-caps/<capture>.png --json
```

## 6. État visuel actuel

Le front renderer actuel qualifie encore exactement :

```text
normal draw 640×360 : tout zéro
resolve 1280×720    : tout zéro
RGBA8 SHA-256       : 0c660f2bd3eff3150dd0040789abe2291613b9af319df870203d4f77a4913a5f
```

Le test pleine taille produit un PNG 1280×720 valide dont les 921 600 pixels
ont RGB=0 et A=0. Cette image est un **test synthétique de l'encodeur à partir
du readback noir connu**, pas une nouvelle capture runtime.

Cette distinction est importante : le pipeline de screencap fonctionne, mais
il rend maintenant visible l'absence de contenu au lieu de la maquiller en
progrès graphique.

## 7. Front renderer suivant

Pour obtenir des captures informatives, le prochain verrou est antérieur à la
screencap :

```text
commandes de draw réelles
→ constantes et ressources du shader
→ résultat non noir du normal draw
→ packing EDRAM non noir qualifié
→ resolve 1280×720
→ screencap d'audit
```

Trois gardes noires restent délibérément en place :

1. `execute_vulkan_normal_draw` exige le digest noir connu ;
2. `materialize_reached_black_edram` n'accepte que le readback noir ;
3. le resolve neutre vérifie le digest noir final.

Elles ne doivent pas être supprimées simultanément. Le meilleur prochain test
est de capturer les constantes et fetches du draw normal, puis de produire un
readback non noir dans une branche expérimentale sans encore l'injecter dans
l'EDRAM guest.

## Validation

- encodeur C++20 : PASS ;
- warnings `-Wall -Wextra -Wpedantic -Wconversion -Wshadow` : PASS ;
- PNG 2×2 : CRC, zlib, RGBA et métadonnées vérifiés ;
- PNG 1280×720 : 3 687 120 octets décompressés vérifiés ;
- SHA-256 RGBA noir connu : exact ;
- compilation syntaxique du publisher avec stubs de l'API projet : PASS ;
- compilation syntaxique de la jointure writeback : PASS ;
- vérificateur d'intégration source : PASS ;
- aucun run PAL réel revendiqué dans cette passe ;
- aucun GitHub Actions ou pull request utilisé ;
- aucun PNG runtime ou octet propriétaire destiné au commit.

## Audit adversarial

- Une image PNG valide ne démontre pas que le pipeline de rendu est correct.
- Le hash RGBA démontre l'identité des pixels capturés, pas leur fidélité au
  hardware Xbox 360.
- Le point de capture est qualifié pour la première jointure atteinte, pas pour
  toutes les futures surfaces de présentation.
- Le fichier de test noir ne doit jamais être présenté comme une frame jouée.
- Un futur résultat non noir fera actuellement échouer les gardes de renderer
  avant la capture finale ; c'est volontaire jusqu'à qualification du packing.

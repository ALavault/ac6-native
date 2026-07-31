# Cycle 366 — les planches de glyphes sont chargées une fois et jamais invalidées

## 1. Mesure

Sonde dans `TextureCache::Texture::WatchCallback` — le point où une écriture
invitée marque une texture périmée — journalisant les deux branches : les deux
bases nommées au cycle 362, et un compte global servant de témoin.

```
rappels de surveillance déclenchés, toutes textures : 11   (témoin vivant)
invalidations des planches de glyphes               :  0
chargements des planches de glyphes                 :  2   (une chacune)
```

**Les deux planches sont chargées une fois et ne sont jamais invalidées.**
Leur contenu au moment du chargement est donc celui qu'elles conservent — et il
échantillonne à zéro.

Le témoin importe : 11 invalidations ont bien lieu pour d'autres textures, donc
le silence sur ces deux bases est un fait, pas une sonde morte.

## 2. Ce que cela désigne

Deux lectures, exclusives :

1. l'invité n'écrit jamais dans ces plages après le chargement — mais alors les
   données seraient déjà correctes au chargement, ce que l'échantillon nul
   contredit ;
2. l'invité **produit** ces textures par un chemin qui **ne déclenche pas la
   surveillance mémoire**.

La seconde est la seule cohérente avec l'ensemble, et elle a un nom probable :
une **résolution GPU** (`resolve`) écrivant dans cette plage. AC6 rendrait son
texte dans une planche de caractères par le GPU, puis l'échantillonnerait. Si le
chemin de résolution n'invalide pas le cache de textures pour cette plage, la
texture échantillonnée reste **vide**.

Cela explique enfin la partition observée depuis le cycle 361 :

| textures | origine probable | rendu |
|---|---|---|
| art d'interface (64x64, 960x264, 224x64, 64x720, 208x48) | données disque, écrites par l'invité | **oui** |
| planches de glyphes (256x256, 320x180) | **produites par le GPU** | **non** |

## 3. Bilan

Quatorze causes éliminées. La cause restante est **une invalidation manquante
sur le chemin de résolution GPU**, et elle est la seule à rendre compte de la
partition art/texte.

## 4. Front suivant

1. Vérifier qu'une résolution vise bien `0x03514000` et `0x028B7000` :
   `VulkanCommandProcessor::IssueCopy` est le point d'entrée, déjà repéré au
   cycle 347 (`command_processor.cpp:4438`).
2. Si oui, vérifier que le cache de textures est invalidé pour la plage résolue.
   C'est un chemin connu et étroit.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

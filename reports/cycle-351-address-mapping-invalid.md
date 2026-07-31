# Cycle 351 — la lecture mémoire des textures est invalide, et le témoin l'a montré

## 1. Ce qui a été tenté

Trancher « mémoire de texture non peuplée » sans instrumenter le cache de
textures : lire directement les octets des textures liées. Les adresses des
constantes de fetch sont **physiques** (`base_address << 12`) ; l'objet de
dialogue du cycle 341 vivait en `0xA3300060`, d'où la traduction supposée
physique `0x03514000` -> virtuel `0xA3514000`.

## 2. Mesure, puis témoin

**Passe défaillante** (écran de sauvegarde), trois textures :

```
0xA3514000  64 octets, non nuls : 0
0xA28E9000  64 octets, non nuls : 0
0xA28B7000  64 octets, non nuls : 0
```

Lecture immédiate : « les textures de l'interface manquante sont vides ».
C'était la réponse attendue depuis cinq cycles.

**Témoin** — la même lecture sur une texture de la passe **qui s'affiche**
(écran-titre, `base=03E21000` et `03CE1000`) :

```
0xA3E21000  64 octets, non nuls : 0
0xA3CE1000  64 octets, non nuls : 0
```

**Également nulles.** Or ces textures se rendent visiblement à l'écran.

## 3. Conclusion

La traduction physique -> virtuel `0x0XXXXXXX -> 0xAXXXXXXX` est **fausse**, ou
la mémoire physique n'est pas adossée au fichier partagé aux mêmes décalages.
La lecture n'observe pas les données de texture. **La mesure est invalide**, et
l'hypothèse « mémoire de texture non peuplée » reste **ouverte, non testée**.

Sans le témoin, ce cycle publiait « les textures de l'interface sont vides,
cause trouvée » — faux, et immédiatement suivi d'un correctif inutile.

C'est la sixième fois en neuf cycles qu'un témoin arrête une observation nulle
avant qu'elle ne devienne une conclusion : `MATE = 0` (343), « la capture
noircit » (344), « capture inerte donc rendu cassé » (345), « ps=0 est le
défaut » (347), et ici. La règle du cycle 324 n'est pas apprise une fois : elle
doit être appliquée **à chaque zéro**, et le seul coût qui la rend fiable est de
mesurer le témoin **avant** d'écrire la conclusion.

## 4. Ce qu'il faut pour reprendre proprement

L'espace d'adressage invité de ReXGlue expose la mémoire physique par plusieurs
miroirs (0x00000000, 0x80000000, 0xA0000000, 0xC0000000, 0xE0000000) aux
propriétés de pavage distinctes. Avant toute nouvelle lecture :

1. établir **quel** miroir le cache de textures utilise réellement, en lisant le
   code de `TranslateVirtual` / du cache plutôt qu'en supposant ;
2. qualifier la lecture sur une texture **connue visible** — le témoin doit
   rendre du non-nul avant qu'une lecture nulle signifie quoi que ce soit.

Sinon, instrumenter le cache de textures après téléversement, où la donnée est
déjà résolue, ce qui évite entièrement la question de l'adresse.

## 5. État

Causes ouvertes, inchangées : mémoire de texture non peuplée, décodage DXT4/5,
traduction du pixel shader `8F1C48BA92C8E43E`.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

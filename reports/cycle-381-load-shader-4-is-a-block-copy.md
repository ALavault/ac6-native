# Cycle 381 — le *load shader* 4 est une copie de blocs, pas un décodeur

## 1. Identification

L'énumération (`texture/cache.h:389`) donne :

```
0 kLoadShaderIndex8bpb    1 kLoadShaderIndex16bpb   2 kLoadShaderIndex32bpb
3 kLoadShaderIndex64bpb   4 kLoadShaderIndex128bpb
```

**L'indice 4 est `128bpb`** : 128 bits par bloc, soit 16 octets — exactement la
taille d'un bloc DXT4/5.

Ce n'est donc **pas un décodeur DXT**. C'est un **copieur de blocs générique**,
qui déplace des blocs de 16 octets de la mémoire invitée pavée vers l'image
hôte. Le contenu n'est jamais interprété ; il est recopié.

Cela corrige le vocabulaire employé depuis le cycle 350 : parler d'un
« décodage `k_DXT4_5` fautif » était inexact. Le compressé reste compressé ; le
GPU le décode à l'échantillonnage. Ce qui peut échouer ici, c'est l'**adressage**,
pas l'interprétation.

## 2. Ce que cela écarte encore

Une hypothèse de décodage — table de couleurs, canal alpha interpolé, ordre des
bits — n'a plus d'objet : rien de tel n'a lieu dans ce chemin.

Reste l'**adressage pavé** : le calcul qui, pour un bloc `(x, y)`, donne son
décalage dans la mémoire invitée.

## 3. Une piste testée et écartée dans la foulée

Le pavage Xenos travaille par tuiles de 32x32 blocs. Si le copieur supposait une
largeur multiple de 32 blocs :

```
1280x720 -> 320x180 blocs ; 320 = 10 x 32   multiple      s'affiche
 256x256 ->  64x64  blocs ;  64 =  2 x 32   multiple      NE S'AFFICHE PAS
 320x180 ->  80x45  blocs ;  80 = 2,5 x 32  NON multiple  NE S'AFFICHE PAS
 960x264 -> 240x66  blocs ; 240 = 7,5 x 32  NON multiple  s'affiche
 224x64  ->  56x16  blocs ;                 NON multiple  s'affiche
```

La largeur en tuiles **ne sépare pas** : un multiple échoue (`256x256`), un
non-multiple réussit (`960x264`). Hypothèse écartée dans le même cycle où elle
est née.

## 4. État

Vingt-cinq causes éliminées. Le défaut reste **localisé** au copieur de blocs
128 bpb pour deux géométries précises, et **non corrigé**.

Les deux singularités du cycle 371 — rembourrage nul pour `03514000`, hauteur en
blocs impaire pour `028B7000` — restent les seules différences structurelles
connues, et aucune ne couvre les deux cas.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

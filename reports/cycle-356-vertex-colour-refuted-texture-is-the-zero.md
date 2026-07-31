# Cycle 356 — la couleur de sommet est réfutée ; c'est l'échantillon de texture qui est nul

## 1. Test exécuté correctement, enfin

Trois tentatives ont échoué pour des raisons de protocole, toutes instructives :

1. cache de shaders sur disque -> le traducteur modifié ne s'exécutait pas ;
2. cache vidé, attente de 75 s puis 150 s -> l'écran-titre, **sans entrée**,
   bascule dans sa **cinématique d'attrait**. Attendre plus longtemps éloignait
   du but au lieu de s'en rapprocher. *(Point signalé par l'opérateur ; sans
   quoi j'aurais continué d'allonger l'attente.)*
3. cache vidé, entrée précoce -> la recompilation des shaders est si lente que
   l'invité n'avait présenté que **4 trames** au moment de la capture.

Protocole correct, dérivé des trois échecs :

```bash
rm -rf build-rt/cache
./ac6recomp ... --ac6_force_white_vertex_colour=true   # 120 s, peuple le cache
                                                        # AVEC le diagnostic
./ac6recomp ... --ac6_force_white_vertex_colour=true   # cache chaud : démarrage
                                                        # rapide, entrée à 33 s
```

## 2. Résultat

Écran de sauvegarde atteint, couleur de sommet `FMT_8_8_8_8` forcée à 1.0 :
**la couche d'interface reste absente.** Fond, panneau, OUI/NON — rien d'autre.
Image identique au cas non forcé.

**L'hypothèse du cycle 353 est réfutée.** La couleur de sommet n'est pas la
cause.

## 3. Ce que la réfutation implique

Le shader défaillant est `oC0 = r0 * r1`, avec `r0` = échantillon de texture et
`r1` = couleur de sommet. Avec `r1` forcé à 1, la sortie devient `oC0 = r0`.
La couche reste invisible, donc :

**`r0` — l'échantillon de texture — est nul ou transparent.**

C'est une déduction, pas une nouvelle mesure, mais elle est directe : les deux
seuls facteurs du produit sont `r0` et `r1` ; `r1` est désormais 1 par
construction ; le produit reste nul ; donc `r0` est nul.

Cela **réhabilite le décodage DXT4/5**, que le cycle 353 avait affaibli en
raisonnant que « la texture n'est qu'un facteur ». C'était juste, mais
l'élimination de l'autre facteur renverse la conclusion : c'est bien le facteur
texture qui vaut zéro.

## 4. État des causes

| cause | statut |
|---|---|
| cible de rendu / mode EDRAM | écarté (348) |
| géométrie hors viewport, test alpha | écarté (349) |
| texture absente ou de taille nulle | écarté (350) |
| texture jamais chargée, mémoire non peuplée | écarté (352) |
| format de sommet non pris en charge | écarté (354) |
| **couleur de sommet nulle** | **réfuté (ici, test contrôlé)** |
| **échantillon de texture nul** | **établi par déduction ; cause à instruire** |
| décodage `k_DXT4_5` | **candidat principal** |

## 5. Front suivant

L'échantillon est nul alors que la texture **est chargée** (cycle 352, témoin
vérifié) et que son descripteur est sain (cycle 350). Restent :

1. le **décodage `k_DXT4_5`** produit des blocs transparents ;
2. l'**échantillonneur** (mode d'adressage, mip, bordure) rend zéro ;
3. les **coordonnées** `r0.xy`, issues de l'interpolateur `o0`, sont hors
   texture — remarquable, car `o0` vient du même vertex fetch que la couleur.

Le point 3 est à tester en premier : il est du même chemin que ce qui vient
d'être réfuté, et forcer les coordonnées à une constante connue le tranche.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

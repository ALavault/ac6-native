# Cycle 398 — deux classes de tracés ; l'une a une échelle NDC nulle

## 1. Mesure

Sonde `ac6_log_vulkan_draws` active, écran de sauvegarde atteint à l'itération 7.
Sur l'ensemble du journal, les tracés se répartissent en **exactement deux
classes**, sans intermédiaire :

| classe | occurrences | viewport | ndc_scale | ndc_offset | primitive |
|---|---|---|---|---|---|
| A | 6917 | 8192×8192 | **0.000, 0.000** | −1.000, −1.000 | prim=1 (point list), 4 sommets |
| B | 994 | 1280×720 | 1.000, −1.000 | 0.000, 0.000 | prim=13 (rectangle list) |

Sur les 400 derniers tracés de l'écran de sauvegarde : 344 en classe A, 49 en
classe B.

## 2. Ce que cela signifie, et ce qui reste incertain

La classe B est le trajet normal : viewport à la taille du tampon, échelle NDC
unitaire, primitive rectangle — la primitive d'interface du Xenos. Le panneau et
les boutons YES/NO, qui **sont** visibles, viennent selon toute vraisemblance
de là.

La classe A a une **échelle NDC nulle sur les deux axes**. Prise littéralement,
elle envoie tout sommet au point NDC (−1, −1) : toute géométrie s'effondre et
ne couvre aucun pixel. Le viewport de 8192×8192 est la valeur de garde
maximale, pas une taille d'écran.

C'est la signature attendue du trajet **sommets pré-transformés** (coordonnées
déjà en espace fenêtre, transformation de viewport désactivée côté invité). Ce
trajet est légitime ; il exige seulement que l'hôte convertisse lui-même. La
question ouverte, non tranchée ici, est de savoir si notre hôte effectue cette
conversion ou s'il applique l'échelle nulle telle quelle.

**Ce n'est pas encore démontré.** Il est établi que 87 % des tracés portent une
échelle NDC nulle ; il n'est pas établi qu'ils disparaissent pour autant.

## 3. Rapport au débordement du cycle 397

Le panneau déborde de l'écran par la droite d'environ 35 %. Un défaut de
conversion des coordonnées pré-transformées produirait précisément ce genre
d'erreur d'échelle. Les deux observations sont compatibles avec une cause
unique, sans que le lien soit prouvé.

## 4. Prochain pas, précis

Lire dans le traducteur de nuanceurs le trajet emprunté quand `ndc_scale` vaut
zéro, et vérifier s'il existe une conversion de secours utilisant la taille
réelle du tampon. C'est une lecture de code, pas une expérience : elle tranche
immédiatement.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

# Cycle 352 — le chargement des textures n'échoue jamais, et cette fois le zéro compte

## 1. Instrument, avec son témoin de vivacité

`VulkanTextureCache::LoadTextureDataFromResidentMemoryImpl` comporte deux
sorties précoces silencieuses : format sans *load shader*, et pipeline de
chargement nul. Une texture qui en sort n'est **jamais peuplée** — elle
s'échantillonne vide pendant que la géométrie qui la référence continue de se
dessiner. C'est exactement le tableau de l'écran de sauvegarde.

Les deux sorties sont désormais bruyantes. **Et surtout**, un compteur de
chargements réussis est posé au même endroit : sans lui, « zéro échec » ne
distingue pas un chemin sain d'un chemin mort.

## 2. Mesure

```
chargements réussis observés    ~3 000   (compteur atteint #3000)
lignes de journal du témoin     23
échecs "NO LOAD SHADER"         0
échecs "NULL LOAD PIPELINE"     0
```

**Le témoin est vivant**, donc le zéro est interprétable. Le chargement des
textures **n'échoue jamais**.

## 3. Ce que cela écarte

- **Texture jamais peuplée faute de load shader ou de pipeline** : écarté.
- **Mémoire de texture non peuplée** : très largement écarté par conséquence —
  les chargements aboutissent. Le cycle 351 avait laissé cette cause ouverte
  après une lecture mémoire invalide ; elle est ici tranchée par le bon bout,
  du côté du cache plutôt que de l'adresse.

Réserve : l'échantillonnage des formats journalisés (8 premiers puis un sur 200)
n'a pas capturé de `format=20` ; c'est un échantillon biaisé, pas un
recensement. Il ne dit rien de plus que ce qui précède, et n'est pas retenu.

## 4. Bilan après six cycles d'élimination

| cause | statut |
|---|---|
| cible de rendu / mode EDRAM | écarté (348) |
| géométrie hors viewport | écarté (349) |
| test alpha | écarté (349) |
| texture absente ou de taille nulle | écarté (350) |
| texture jamais chargée | **écarté (ici, témoin vérifié)** |
| mémoire de texture non peuplée | **écarté par conséquence (ici)** |
| décodage DXT4/5 incorrect | **ouvert** |
| pixel shader `8F1C48BA92C8E43E` mal traduit | **ouvert, candidat principal** |

Deux causes restent sur huit. Toutes deux relèvent de la **phase P3**
(traduction Xenos), pas du chemin de commandes ni de la gestion mémoire.

## 5. Front suivant

1. Vider le shader traduit `8F1C48BA92C8E43E` et vérifier qu'il écrit une
   couleur non nulle, avec `1899F02DC6758D8F` (écran-titre, qui aboutit) comme
   témoin obligatoire.
2. Si le shader est sain, comparer le rendu d'une même texture `k_DXT4_5` entre
   notre runtime et l'oracle headless, désormais disponible et reproductible.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

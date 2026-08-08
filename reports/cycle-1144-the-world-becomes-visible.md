# Cycle 1144 — le monde retail devient visible, et se montre faux

Date : 2026-08-08. Première étape JV du plan post-JF.

## Qualification

- Charge utile : nœud racine de scénario Mission 01,
  SHA-256 `51c10abe543ec1b8210bf089704db640003662fcb91f3b8dbaa091ec45ac6d45`.
- **Statique et produit natif seuls.** Aucun oracle.

## Le problème que ce cycle attaque

Le produit a deux moitiés qui ne se rencontrent pas. Le chemin des manifestes
dessine — mais ses transformations, ses matériaux et ses textures sont
**synthétisés** par `tools/make_mission01_native_manifest.py`, et il dessine
trois objets. Le chemin retail a le monde — 230 unités, quatre sous-missions,
tout depuis le conteneur — et ne dessine **que le HUD** :
`run_retail_session` n'appelait ni le rasteriseur ni la présentation.

## Ce qui est ajouté

`NativeRenderTarget::draw_world_marker` : une position monde, projetée par **les
deux mêmes chemins de caméra que la géométrie** — les lignes qualifiées
`c218`–`c221` quand une caméra est fournie, la base du runtime sinon — et tracée
avec test de profondeur.

Ce n'est pas de la géométrie et l'en-tête le dit : pas de matériau, pas de
texture, pas de topologie, et **aucune capture qui en contient ne peut être
offerte comme parité visuelle**. C'est une voie de diagnostic, et elle existe
pour une raison précise : la session retail sait où sont 230 unités bien avant
de savoir à quoi elles ressemblent, et *une position qu'on ne voit pas est une
position qu'on ne peut pas contrôler*.

`RetailSession::render_world_markers` colore chaque unité par **l'octet de
faction que la table retail lui a donné** — la partition 140/42/48 de la
Mission 01, rien de choisi à la main — et distingue le joueur.

## Le résultat, et il est négatif

```
world_markers_live      10
world_markers_debrief   11
world_marker_writes    130
active_units           230
```

**Dix unités sur 230 atteignent l'écran.** Les autres sont à l'origine ou tout
près : leurs positions sont les triplets `Obj` **sans leur repère**. Le cycle
1142 a trouvé la chaîne de placement — bloc de données → `+0x184` → `+0xA0` →
validation — mais pas le repère auquel le triplet est relatif ; `0x8229AF80`
teste `[entité+0x188]` avant d'écrire, et ce parent n'est pas identifié.

C'est exactement ce que la voie de diagnostic devait produire : **le défaut
devient visible au lieu d'être décrit.** Le rapport de dette disait « la valeur
est juste et son repère ne l'est pas » ; la capture le montre.

## Ce que cela n'établit pas

- **Rien sur l'apparence.** Un marqueur n'est pas un modèle.
- **Rien sur la caméra.** Le chemin de repli du rasteriseur porte une focale
  codée en dur (60°, plan lointain 4096) ; la vraie caméra de vol reste ouverte.
- **Que dix soit le bon compte.** C'est le compte actuel, pas une cible.

## Décision de cycle

Le marqueur est ajouté au rasteriseur plutôt qu'à la session, parce que la
projection doit rester **la même que celle de la géométrie** : deux projections
divergentes rendraient la comparaison future impossible. Et il compte ses
écritures, pour que la capture soit mesurée avant d'être écrite, comme le veut
la discipline déjà en place sur ce paquet.

`ctest 24/24`, la porte JF reste verte — les empreintes des artefacts cités ont
été mises à jour, la capture ayant changé.

# Cycle 344 — « MATE = 0 » était un faux indice ; la capture intégrée noircit l'écran

## 1. Rétractation : `frame guest MATE = 0` ne signale rien

Le cycle 343 désignait `frame guest MATE: 0` comme « la piste la plus
prometteuse » sur l'écran de sauvegarde.

Lecture du code : le champ est
`diagnostics.capture_material_identity_draw_count`
(`src/ac6_native_graphics_overlay.cpp:154`) — un compteur du **mode capture**.
La surcouche annonce par ailleurs `capture active: no`.

Vérification sur l'écran-titre, où le texte **se rend correctement** :

```
capture active: no
capture draws / clears / resolves: 0 / 0 / 0
frame guest MATE / backend issue / success / host draw: 0 / 44 / 43 / 43
                                              ^ 0 ici aussi
```

**MATE vaut 0 sur les deux écrans**, parce que la capture est désactivée, pas
parce qu'un matériau manque. L'indice du cycle 343 est **retiré**.

Leçon, déjà rencontrée au cycle 332 avec des comptages de `REX_FATAL`
contradictoires : **vérifier qu'un compteur à zéro est alimenté avant d'en
faire un signal.** Un zéro n'est une information que si le canal est vivant —
c'est la règle du cycle 324, appliquée ici à un compteur de surcouche.

## 2. Attribution des 56 dessins : tentée, non obtenue

L'outil adéquat existe déjà : `REXCVAR_DEFINE_BOOL(ac6_render_capture, ...)`
(`src/d3d_hooks.cpp:15`), qui alimente précisément les compteurs
`capture draws / clears / resolves`, `capture indexed / shared / primitive` et
l'identité de matériau — l'attribution demandée.

Exécution avec `--ac6_render_capture=true` :

```
lignes de journal        2 503
REX_FATAL                0
captures écran-titre     9 865 octets
captures écran sauvegarde 9 865 octets  (identiques)
```

Deux PNG de 9 865 octets pour du 1280x720 sont des images **unies**. Activer la
capture **noircit le rendu dès le départ**, sans erreur ni piège. L'instrument
est donc inutilisable en l'état pour cette mesure, et l'attribution des 56
dessins **n'est pas obtenue**.

À noter, honnêtement : cette exécution omettait
`--frame_loop_telemetry_interval`, d'où l'absence de lignes de compteurs ; cela
n'explique pas les images unies, qui viennent bien du mode capture.

## 3. État de la question

L'inférence du cycle 343 — couche « émise mais invisible » plutôt qu'absente —
**reste une inférence**, fondée sur le seul fait mesuré et non contesté :

| écran | dessins / trame | texte visible |
|---|---|---|
| titre | 44 / 43 / 43 | oui |
| sauvegarde | 56 / 56 / 56 | non |

Plus de dessins, moins de contenu. Rien dans ce cycle ne la renforce ni ne
l'affaiblit ; le cycle a en revanche supprimé un faux appui.

## 4. Front suivant

1. Déterminer pourquoi `ac6_render_capture=true` noircit le rendu — c'est un
   défaut en soi, et il bloque le seul instrument d'attribution existant.
2. À défaut, attribuer les dessins par une autre voie : journaliser par dessin
   la texture et le programme liés, au niveau du backend, plutôt que par le
   mode capture.

P1.3 non franchie. La première mission ne se joue pas.
`recompiler-generated` n'est pas `verified`.

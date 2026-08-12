# Checkpoint 4i — inventaire shader Mission 01, sans contrat SPIR-V promu

Date : 2026-08-12

L'inventaire courant relie les indices historiques suivants au cône Mission 01
mais ne fournit pas encore un shader livrable :

| rôle observé | identifiant | artefact | statut |
|---|---|---|---|
| monde/map-object, bridge | VS `472913F460D4B446`, PS `8F1C48BA92C8E43E` | `reports/code/shader_472913F460D4B446.ucode.vert`, `reports/code/shader_8F1C48BA92C8E43E.ucode.frag` | indice bridge, non promu |
| monde avec lumière, bridge | PS `D5B4F4A878949938` | dump cycle 1017, SHA `e30a417e77071d9b42779afa61849fbdd9d94a6c086e3e2e368409d8f047f5bf` | contrat statique seulement |
| écran-titre, témoin bridge | PS `1899F02DC6758D8F` | `reports/code/shader_1899F02DC6758D8F.ucode.frag` | témoin historique |
| transport Vulkan actuel | fixtures SPIR-V triangle/textured/clip | `reconstruction/ace-combat-6/tests/fixtures/` | tests uniquement |

Les captures bridge indiquent une soumission réelle (372 draws D5B4 sur la
frame instrumentée) et un shader `8F1C` syntaxiquement valide, mais la trace
qualifiée v2 stock ne contient ni ucode, ni constantes, ni hash de pipeline
retail. Les SPIR-V des fixtures ne sont donc pas des approximations de ces
shaders et aucun n'est attaché à `play`.

La frontière suivante est explicitement nommée : une capture oracle v2
reproductible doit fournir le premier couple shader/constantes et sa cible de
resolve dans la même fenêtre. Avant cela, porter Xenos→IR→SPIR-V ou choisir le
couple historique `472913/8F1C` serait une hypothèse statique non recevable.

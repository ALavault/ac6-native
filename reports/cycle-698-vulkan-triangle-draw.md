# Cycle 698 — premier draw Vulkan SPIR-V et readback de pixels

Date : 2026-08-03  
Périmètre : reconstruction native AC6, backend Vulkan AC6-owned, sans oracle.

## Résultat

Le backend ne s'arrête plus au clear : il accepte deux modules SPIR-V fournis
par l'appelant, crée un pipeline graphique compatible avec la render pass de la
cible, alloue un vertex buffer hôte, soumet un triangle et relit les pixels.
Le smoke test utilise un vertex shader `vec2 → gl_Position` et un fragment
shader vert constant. Sur la cible 8×8, le pixel central est exactement
`[0,255,0,255]` après le draw.

Cette API est volontairement générique et sans descripteur : elle prouve la
chaîne `SPIR-V → pipeline → vertex buffer → vkCmdDraw → readback`, mais ne
prétend ni reproduire les shaders Xenos AC6, ni brancher les MATE/NDXR/NTXR,
ni afficher une frame retail.

## Contrat ajouté

```text
create_triangle_pipeline(render_target, vertex_spirv, fragment_spirv)
draw_triangle(pipeline, render_target, three_vertices)
readback_render_target(render_target)
release_triangle_pipeline(pipeline)
```

Le pipeline vérifie l'identité de la render pass avant le draw. Les modules
SPIR-V temporaires sont détruits après création du pipeline ; le pipeline et
son layout sont libérés avant la cible dans le test. Le test conserve aussi
les invariants précédents : descriptor avant upload refusé, upload du NTXR,
clear rouge relu et destruction des ressources.

Les shaders de fixture et leurs mots SPIR-V sont dans
`tests/fixtures/vulkan_triangle.{vert,frag}` et
`tests/fixtures/vulkan_triangle_spirv.h`; aucun asset retail n'est ajouté.

## Validation

```text
CTest : 51/51 passed
Total Test time : 40.82 s
ac6-vulkan-backend-tests : pass
git diff --check : pass
```

Hashes source :

```text
include/ac6/vulkan_backend.h         7883197363d4c6ae8fe4466d89ab83244f021b62f3cb5f155df0c3715a1239c5
src/vulkan_backend.cpp               08307974677a735f254fc9146e7ed4750ab2c6166c9a2af3910f0abd860e4c0c
tests/vulkan_backend_tests.cpp       00e6a56f80533d55e1541a60067dc8a4185bf1810e8d5596bc0a6202113a065f
tests/fixtures/vulkan_triangle.vert  7cd3024342e0e98c9b5707fba2cd9a7770035c6670c723d3e5afbe403e11785f
tests/fixtures/vulkan_triangle.frag  86ed0b99c7569133fe9270106a56e36cd850633eb753ff89645282e280925ba5
tests/fixtures/vulkan_triangle_spirv.h f51331ab6b6c9d1cb55769f295e3258071f31f9dad68bed436e0279f5c9b910c
CMakeLists.txt                       0b23063182ccc15a15ef7886b50409bae71f922ffe401cf8d104bd9ad447296b
```

## Frontière suivante

Le draw générique permet maintenant de mesurer un shader AC6 traduit sur une
cible headless. La suite doit remplacer la fixture par un module issu du
contrat shader Xenos qualifié, puis ajouter vertex/index buffers et layouts
BC1/BC3 par ressource. La progression Mission 2 reste séparée : selector 2 →
DPL 10 → DATA.TBL[10] est seulement une route statique et la sauvegarde retail
disponible reste vide.

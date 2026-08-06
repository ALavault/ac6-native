# Cycle 705 — draw mesh indexé Vulkan

Date : 2026-08-03  
Périmètre : upload vertex/index, triangle-list explicite et rendu Vulkan
texturé.

## Résultat

`VulkanCampaignBackend::draw_indexed_mesh` ajoute un seam GPU séparé du helper
triangle historique :

* deux buffers host-visible distincts pour vertices et indices ;
* vérification des handles, render pass, pipeline texturé et bornes d’indices ;
* rejet des index-count non multiples de trois, car ce point d’entrée choisit
  explicitement `VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST` ;
* `vkCmdBindIndexBuffer` + `vkCmdDrawIndexed`, puis libération déterministe ;
* readback de la couleur texturée sur un quad indexé.

La fixture vérifie aussi qu’un indice 4 dans un vertex stream de taille 4 et
une liste de deux indices sont rejetés sans soumission. Le flux utilisé est
`position.xy + uv`, identique au shader SPIR-V déjà qualifié ; le `z` du
contrat NDXR et les `primitive_flags` ne sont volontairement pas interprétés.

## Validation

```text
ac6-vulkan-backend-tests : pass
quad indexé texturé + readback : pass
indices hors bornes/non-triangle : rejetés
CTest complet avec AC6_ASSET_ROOT : 53/53 pass, 40.49 s
whitespace check : pass
```

Hashes :

```text
include/ac6/vulkan_backend.h 5c1cd378334b91ed99e5c7cb83cb673e35d260ca466a8697c7fd31706188f815
src/vulkan_backend.cpp        af2846b59a1534abb1707aac19af650cbd473521bffe244e1e96f720f24673fd
tests/vulkan_backend_tests.cpp a3ebf02400a64c75f4909f4fc1f2444c67e09aca5e9719d91413f0bb98e458ee
```

## Limite explicite

Ce draw est un contrat de transport et de topologie choisi par le caller. Il
ne prouve pas la topologie retail de `primitive_flags`, le layout vertex Xenos
3D, la profondeur, les shaders AC6, ni la présentation/swapchain. Il ne change
aucune conclusion sur le HUD/monde noir, les avions blancs ou la sauvegarde
interactive Mission 1/Mission 2.

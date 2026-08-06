# Cycle 707 — adaptateur CampaignMesh → Vulkan

Date : 2026-08-03  
Périmètre : raccord du contrat CPU NDXR au draw GPU xyz+UV.

## Résultat

`VulkanCampaignBackend::draw_campaign_mesh` accepte directement un
`CampaignMesh`, copie ses vertices dans le layout Vulkan xyz+uv du cycle 706,
et réutilise la validation/indexation du cycle 705. Le chemin ne transforme
pas les `primitive_flags`, ne projette pas les coordonnées et ne sélectionne
pas de texture à partir de l’identifiant : ces décisions restent aux couches
qualifiées correspondantes.

La fixture exécute le chemin complet :

```text
CampaignMesh CPU (4 vertices / 6 indices)
  → pipeline SPIR-V xyz+uv
  → vertex/index staging Vulkan
  → vkCmdDrawIndexed
  → readback texturé
```

Le pixel central reste `[127,127,127,127]`; les validations invalid-index et
non-triangle du chemin bas niveau restent actives.

## Validation

```text
ac6-vulkan-backend-tests : pass
CampaignMesh → Vulkan → readback : pass
CTest complet avec AC6_ASSET_ROOT : 53/53 pass, 39.82 s
whitespace check : pass
```

Hashes :

```text
include/ac6/vulkan_backend.h 0d9ab85396f514899a962e9cb21d63cdec57c2e8a4711f3f8bcf445dc54b14e7
src/vulkan_backend.cpp        e00654b27903e999b64e46239c5e6b560411ba4639c13ec4051fa7ce5eb61d27
tests/vulkan_backend_tests.cpp 8c7d4f6b232650a580fd7c8f1163a1adf6f939c088fa4de9d14cfa07f3268637
```

## Limite explicite

Le raccord est volontairement neutre : il ne prouve ni que les coordonnées
NDXR sont déjà en clip-space, ni la topologie des `primitive_flags`, ni le
mapping MATE/texture, la profondeur ou le shader AC6. Le prochain seam doit
qualifier caméra/matrices et présentation avant toute affirmation de monde
Mission 1 rendu.

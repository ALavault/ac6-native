# Checkpoint 1 — récupération du backend Vulkan

Statut : **fermé pour le transport GPU générique** le 11 août 2026.

## Résultat

- Le snapshot historique non Git est qualifié par 15 couples taille/SHA-256 dans
  `analysis/vulkan/backend-recovery-v1.json`.
- Seul le fixture SPIR-V générique a été importé octet pour octet. Les mécanismes
  device, ressources, pipelines, commandes, readback, surface et swapchain ont
  été adaptés derrière une API AC6-owned sans mémoire invitée.
- Les contrats historiques `CampaignVulkan*`, leur runtime et leurs tests retail
  n'ont pas été importés.
- Les mesh/index buffers sont persistants. Le chemin `draw_indexed` n'alloue pas
  de staging par draw; le staging de readback reste borné à l'appel explicite de
  l'oracle de test.
- Les 70 fichiers associés aux 40 rapports Vulkan historiques du périmètre sont
  classés : 15 rapports `reproduit`, 14 `indice`, 11 `superseded`. Un rapport
  sans artefact courant ne ferme aucun gate.

## Validations

- Build propre Ninja `RelWithDebInfo` : succès.
- CTest headless avec `SDL_AUDIODRIVER=dummy` sous Xvfb : 76/76 terminés,
  75 succès et 1 skip qualifié (`ac6-retail-frontend-resources`).
- `ac6-vulkan-backend` : clear RGBA8, cible profondeur D32, pipeline SPIR-V,
  deux draws indexés, readback déterministe et destruction des ressources.
- `ac6-vulkan-surface-smoke` : instance, surface SDL, device, swapchain, clear,
  transfert du frame CPU et présentation multi-image.
- Audit du manifeste avec le snapshot source : 15 fichiers et 70 chemins
  vérifiés, hashes conformes.
- JF : pass. Artefacts : 146/146. Adresses : 321/321. Dérivations : 52/52.
- Frontière produit source et ELF : pass. Tests Python : 102/102. Complexité et
  `git diff --check` sur le périmètre : pass.
- Paquet TGZ : 86 entrées auditées; staging : 80 fichiers, sans artefact retail,
  runtime de preuve ni dépendance interdite.

## Risques résiduels

- Le SDK local ne fournit ni `spirv-val` ni la couche
  `VK_LAYER_KHRONOS_validation`; la création réelle des modules/pipelines par
  le driver NVIDIA qualifie le fixture, pas encore l'inventaire shader retail.
- Les uploads NTXR/BC1/BC3, descriptors et états Xenos restent des indices, pas
  des mécanismes récupérés.
- Le chemin interactif courant présente encore le raster CPU. Le remplacement
  par des soumissions directes est le checkpoint 4 et ne peut être revendiqué
  ici.
- Aucune capture de l'ancien runtime `Campaign*` n'est recevable comme preuve
  produit; l'oracle reproductible et sa route Mission 01 restent le checkpoint
  2.

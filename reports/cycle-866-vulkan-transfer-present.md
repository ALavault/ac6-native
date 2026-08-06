# Cycle 866 — swapchain transfer contract

Le chemin `VulkanFramePresenter::present_frame` copie désormais les pixels
RGBA8 vers les images de swapchain avec une déclaration explicite de
`VK_IMAGE_USAGE_TRANSFER_DST_BIT`. La création échoue proprement si la surface
ne supporte pas simultanément le transfert et l'usage couleur. Un test natif
vérifie aussi la conversion ARGB interne vers RGBA8 avant upload.

Validation :

- build CMake complet réussi ;
- CTest : `1/1` réussi ;
- smoke SDL3/Vulkan sous Xvfb + `SDL_AUDIODRIVER=dummy` : code retour 0.

La première frame Mission 01 reste conditionnée par un manifeste retail
qualifié et ses tranches géométriques externes ; aucun asset n'est embarqué.

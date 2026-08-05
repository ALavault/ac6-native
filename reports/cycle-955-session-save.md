# Cycle 955 — format de session combiné

`SessionSaveStore` joint `RuntimeSnapshot` et `CampaignSaveSnapshot` dans un
format borné `AC6SESS`. Les formats historiques `AC6SAVE` et `AC6CSAV` ne
sont pas modifiés. Les valeurs flottantes doivent être finies, l’accumulateur
reste dans une période fixe, les slots et les missions sont uniques et triés,
et les compteurs sont bornés. L’écriture utilise un fichier frère temporaire
et un remplacement atomique; la lecture est transactionnelle.

Le test vérifie le round-trip des deux sous-états, la restauration par
`CampaignProgression`, le rejet d’un slot nul et la conservation de l’état
après fichier corrompu.

Validation : build CMake, CTest (`5/5`) sous Xvfb avec
`SDL_AUDIODRIVER=dummy`, et smoke SDL3/Vulkan (`swapchain_images=3`).

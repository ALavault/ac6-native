# Cycle 953 — persistance campagne native

`CampaignSaveStore` persiste les enregistrements de progression dans un
format borné `AC6CSAV` distinct de `AC6SAVE`. Les slots et missions sont
triés à l’écriture; doublons, identifiants nuls, compteurs hors limite,
troncature et octets résiduels sont refusés. L’écriture passe par un fichier
frère temporaire puis remplacement atomique. La lecture construit une table
temporaire et conserve l’état précédent si elle échoue.

Le test couvre round-trip, slot nul, fichier corrompu et conservation de
l’état. Build CMake, CTest (`4/4`) sous Xvfb avec `SDL_AUDIODRIVER=dummy` et
smoke SDL3/Vulkan (`swapchain_images=3`) passent.

Limite déclarée : le snapshot de vol `AC6SAVE` et le snapshot campagne
`AC6CSAV` ne sont pas encore regroupés dans un format de session unique.

# Cycle 873 — bornes SDL vers InputFrame

Le chemin `SdlInputAdapter` → `InputFrame` est maintenant couvert sur les
bornes SDL : axe pitch inversé, yaw à `INT16_MIN`, gâchette à `-32768` et
mapping d'axe invalide. Les événements bouton continuent de produire les
événements natifs via la table de mapping.

Validation : build CMake, CTest `1/1`, smoke SDL3/Vulkan sous Xvfb : OK.

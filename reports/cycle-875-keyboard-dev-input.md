# Cycle 875 — clavier de développement SDL

`SdlKeyboardMapping` fournit un mapping distinct du contrôleur (W/S, A/D,
Q/E, R/F) avec validation des scancodes et absence de doublons.
`SdlInputAdapter` traduit key-down/key-up en axes `InputFrame`; les tests
couvrent pitch et throttle. Ce chemin est destiné au clavier de développement
et ne remplace pas les mappings retail qualifiés.

Validation : build CMake, CTest `1/1`, smoke SDL3/Vulkan sous Xvfb : OK.

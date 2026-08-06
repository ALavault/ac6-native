# Cycle 808 — RenderAssets et frontend natif

Le renderer consomme maintenant `WorldFrame` et `RenderAssets` séparément ;
les IDs 9 et 119 doivent être résolus avant toute soumission. Un
`FrontendController` fournit le flux déterministe `Title → NewGame → Briefing
→ Hangar → Loading → Mission`, puis refuse toute avancée supplémentaire.

Validation : build CMake et CTest `1/1` réussi.

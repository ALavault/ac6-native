# Cycle 929 — paramètres frontend PAL explicites

`FrontendController` porte maintenant `FrontendSettings` et refuse toute
configuration non qualifiée pour le parcours produit : difficulté Normal,
contrôles Normal et langue English. Le smoke frontend configure explicitement
ce profil avant Title → New Game → Briefing → Hangar → Loading → Mission; les
valeurs Easy/Expert ou langues non anglaises restent disponibles comme types
mais sont refusées tant que leurs contrats retail ne sont pas qualifiés.

Validations : CTest normal 3/3, ASan/UBSan 3/3, package audit pass.

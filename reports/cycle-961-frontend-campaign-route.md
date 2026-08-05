# Cycle 961 — route frontend campagne

`FrontendController` accepte maintenant une frontière optionnelle
`CampaignProgression`. La sélection refuse une mission verrouillée, le passage
New Game → Briefing appelle `enter_briefing`, le loadout est obligatoire au
Hangar, et le passage Hangar → Loading appelle `begin`; l’écran Mission n’est
atteint qu’après l’état campagne `Active`.

Le chemin sans campagne reste disponible pour les fixtures développeur. Le
test couvre le parcours complet, le rejet du loadout hors Hangar et le rejet
d’une mission verrouillée. Build, CTest (`5/5`) sous Xvfb avec
`SDL_AUDIODRIVER=dummy`, smoke SDL3/Vulkan et audit campagne passent.

Les paramètres de menu retail et les mappings des missions non qualifiées
restent hors de cette frontière générique.

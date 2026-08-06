# Cycle 795 — route d’amorçage explicite (binaire qualifié)

Le binaire `401cc83b491bf1f594df95168a54209e8c33b9eaa09224cd6c35742b1cd94f6a`
et le profil copié du cycle 782 ont été relancés avec `SDL_AUDIODRIVER=dummy`,
`user_data_root` explicite et les cvars d’observation UI/entrée.

Des touches explicites à 1, 20, 40, 60, 80 et 100 frames ont été injectées.
Le runtime a publié 105 frames et l’écran reste sur l’introduction Project Aces;
aucun `ac6-save-outer` ni `type28=30` n’est observé. Ce résultat distingue la
route d’entrée du contrat gameplay : le profil est bien monté, mais le passage
des écrans légaux n’est pas encore reproductible avec ce harness.

Contrôle : le log confirme le profil `reports/logs/cycle-795-qualified-explicit-boot/user-data`
et les appels de polling sur `0x8290DE3C`; aucune preuve Mission 01 n’est retenue.

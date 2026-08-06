# Cycle 911 — filtre nearest/linear

Le sampler honore maintenant le filtre déclaré : nearest retourne le texel
discret, linear effectue une interpolation bilinéaire déterministe. La fixture
P6 2×1 vérifie l'interpolation rouge/bleu à mi-course, en plus des modes
wrap/clamp.

Validations : CTest normal 3/3 et ASan/UBSan 3/3.

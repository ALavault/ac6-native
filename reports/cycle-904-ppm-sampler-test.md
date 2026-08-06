# Cycle 904 — test sampler image externe

Le contrat PPM externe est maintenant couvert par un test natif : fixture P6
2×1, identité FNV vérifiée, puis deux coordonnées UV nearest/wrap retournent les
pixels rouge et bleu attendus. Le test protège le chemin image→lookup UV sans
impliquer les archives retail.

Validations : CTest normal 3/3 et ASan/UBSan 3/3.

Limite inchangée : le PPM est une sortie de décompression hors ligne ; le
binding final doit encore sélectionner les NTXR/mips via MATE/GIDX et appliquer
les permutations shader retail.

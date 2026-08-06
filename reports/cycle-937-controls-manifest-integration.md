# Cycle 937 — profil SDL dans le manifeste

Les manifests peuvent maintenant déclarer une ligne `controls`. Le frontend
smoke charge et valide alors le profil SDL externe strict avant la transition
Title → Mission. Sans cette ligne, le mapping compilé reste disponible pour les
smokes développeur existants.

Le chemin ne copie que le TSV fourni par l’utilisateur dans le répertoire de
manifeste externe ; aucun mapping retail n’est embarqué dans le binaire.

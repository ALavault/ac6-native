# Vérification finale

- [x] La capture est opt-in via `AC6_DEMO_AUDIT_SCREENCAP_DIR`.
- [x] Le répertoire doit exister.
- [x] Les sorties sont créées exclusivement, sans écrasement.
- [x] Le point de capture suit le writeback guest et sa relecture validée.
- [x] Le SHA-256 RGBA8 est recalculé avant publication.
- [x] Le PNG préserve RGBA8 et l'alpha.
- [x] Les CRC PNG et l'Adler-32 zlib sont testés.
- [x] Un test 2×2 couvre couleurs et alpha partiel.
- [x] Un test 1280×720 couvre plusieurs blocs DEFLATE stockés.
- [x] Le digest du readback noir connu est reproduit exactement.
- [x] Le sidecar est vérifiable indépendamment.
- [x] Un échec PNG supprime le sidecar prépublié.
- [x] `gameplay_screenshot_claim` reste faux.
- [x] Aucun GitHub Actions ou pull request n'est requis.
- [x] Aucun PNG runtime, XEX ou actif propriétaire n'est destiné au commit.
- [ ] Une capture PAL process-fresh non noire n'est pas encore disponible.

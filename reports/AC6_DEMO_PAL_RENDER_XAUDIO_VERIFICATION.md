# Vérification finale

- [x] La cible est la démo campagne PAL.
- [x] Le writer unique de `device+0x5460` est établi.
- [x] Le payload `(event 17, channel 6)` est vérifié sur les instructions PAL.
- [x] Le callback renderer est relié au service 47 et à son initialiseur.
- [x] Les dix appels directs à `KeSetEvent` sont recensés.
- [x] Les tables audio par processeur sont reconstruites.
- [x] La sélection par `r13+0x10C` et la propagation du null sont vérifiées.
- [x] Le RTTI PAL de `CModeTaskRanking` est vérifié indépendamment.
- [x] Le bridge natif est joint au PCR partagé et au handler d'affinité non stateful.
- [x] Le vérificateur Python passe sur l'image mémoire exacte.
- [x] Aucun ZIP ni octet propriétaire n'est publié.
- [ ] Le dispatcher concret de `(17,6)` reste à capturer.
- [ ] Le CPU exact du callback XAudio reste à observer dans une même trace.

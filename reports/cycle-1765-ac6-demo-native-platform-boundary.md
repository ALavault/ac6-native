# AC6 `ac6-demo-native` domaine 2 — plateforme déterministe cycle 1765

Verdict : **PARTIAL/NO-GO**, `supported=false`.

Le commit `ef827eeb07e29169a77ffb5fbc645414c5481edf` ajoute la
première frontière du domaine 2 sans ouvrir le runtime :

- trame XInput typée (`uint16` boutons, `uint8` gâchettes, `int16` axes,
  connexion explicite), conforme aux bornes `ac6-agent-action/v1` ;
- une action acceptée avance exactement un tick ; temps de simulation dérivé
  par arithmétique entière à 60 Hz, sans horloge murale ;
- PRESENT uniquement notifié par le consommateur guest/runtime, au plus une
  fois par tick ; aucun compteur synthétisé à intervalle fixe ;
- observation immutable `tick/PRESENT/time/input`, reset neutre déterministe,
  aucun fallback HID.

Sources de contrat : `strict-policy.json` SHA-256
`dab6079652716eb5c72a14f99209ca71cb696a36081a8d7a5ebbfe98ccb6b329`
(`AC6RTPLY-v4`, 60 Hz, intervalle nominal 2) et `protocol/v2.py` SHA-256
`829f7ae67309db1cf45570f24b91a8c15194c82e5b213b11b230e51c6d1e9d5e`
(`ac6-agent-action/v1`). Aucun C++ généré n’est copié.

## Validation

```text
cmake configure/build -j16                                      PASS
SDL_AUDIODRIVER=dummy xvfb-run -a ctest                         4/4 PASS
ac6-demo-native-platform (deux routes 600 ticks identiques)     PASS
complexity audit                                                PASS (14 fichiers)
install canonique; absence de bin/bin                           PASS
```

La route de référence livre `buttons=16` exactement au tick 252 et 174
PRESENT guest-notifiés entre les ticks 254 et 600 ; ce sont des tests de
contrat, pas une sémantique START ni une preuve de cadence guest.

Restent absents : journal natif `AC6RTPLY-v4`, IPC `demo-native`, scheduler
guest, mémoire Xenon, renderer, observations gameplay, frontend, mission et
terminal. Le domaine 2 reste donc PARTIAL et aucune comparaison recomp/native
runtime n’est promue.

# Checkpoint 4k — tentative de replay d’entrée oracle, borne vidéo confirmée

Date : 2026-08-12

La route `scripts/ac6-oracle-mission01-controlled-sortie-replay.steps` a été
relancée avec le binaire oracle qualifié
`e5df0f9ddf07945dd667d0f891e194bcd707445773bd55f43a3738e3b643647c`, le XEX
PAL canonique et `--trace-input /tmp/ac6-oracle-input-a.tsv` (SHA-256
`523a0535f11785433436101f8a6bba56166c4cfcd6e3a8758c86cf4ad6b93e47`).

Le profil privé est resté vivant 533,69 s sans fatal, mais a de nouveau bloqué
après l’écran de langue avant la transition campagne : 59 étapes exécutées sur
90, aucun `mission01-execution-v2.arm`, aucune trace v2. Le log finit sur des
underruns XMA `missing-next-packet-for-split-frame`; le processus a été arrêté
par le harness possédé (`game_status=-9`, `xvfb_status=0`) et `/dev/shm` a été
nettoyé selon le manifeste.

Cette tentative confirme la borne déjà classée `superseded` pour l’audio/
callback vidéo. Le TSV armé n’est pas promu et aucune comparaison oracle↔natif
nouvelle n’est produite. L’extracteur d’entrée reste toutefois qualifié par la
reproduction exacte du TSV A et ses tests unitaires.

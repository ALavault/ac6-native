# Cycle 1517 — identité de cache dans la sauvegarde retail

## Delivered

Le format `SessionSaveStore` passe en version 10 pour ajouter le digest
SHA-256 de l’index de contenu au snapshot. `retail play --save` renseigne ce
digest avec l’index du cache ouvert ; les sauvegardes génériques et les
versions 1–9 migrées conservent un digest nul explicite. Les checkpoints,
campagne et chargement atomique restent inchangés.

## Validation

- `ac6-session-save-tests` : passe avec un digest non nul round-trippé.
- `ac6-retail-session` : passe après l’évolution de la structure.
- Les fixtures v1/v5 de migration restent acceptées.

## Boundary retained

Le lecteur de sauvegarde expose l’identité mais aucun appelant de reprise
retail ne compare encore ce digest à un cache fourni par l’utilisateur ; cette
garde doit être ajoutée au chemin de reprise persistant. Le replay retail
reste en version 2 et ne porte pas encore graines/checkpoints/digest final.

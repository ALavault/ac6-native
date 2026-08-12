# Cycle 1552 — audit Banjo-Tooie XBLA et fork ReXGlue

## Verdict

Banjo-Tooie apporte une correction opérationnelle déjà cohérente avec le
harness AC6 : lorsque SDL ne trouve aucun périphérique audio, réinitialiser le
sous-système avec le backend `dummy` permet au callback de continuer à drainer
les frames. Cela justifie de conserver `SDL_AUDIODRIVER=dummy` dans les runs
AC6 Xvfb ; ce n'est pas une preuve de cadence retail ou de synchronisation A/V.

Le reste du patch est surtout spécifique au bring-up. La queue clavier de
`GetKeystroke`, la licence XContent forcée, l'auto-sélection d'un bouton
« offline » et l'extracteur STFS ne doivent pas être repris. Plusieurs cas
montrent pourquoi : résultat dépendant de l'ordre des drivers, événements
produits seulement lors d'un `GetState`, politique UI fondée sur une chaîne
localisée, et import sans identité ni hashes.

Aucune lane M01 n'est fermée par cet audit.

## Provenance

| Élément | Révision qualifiée | Arbre / licence |
|---|---|---|
| `iron-jay/banjo_tooie_xbla_recomp` | `87eb2e0fd046a8c1e21765ddd6c6755bac2e0d9b` | `8b5e814fa806c431d0a8a08491f83cbd0e948b1d`, aucune licence racine |
| `iron-jay/rexglue-sdk` | `035aa253bdf8ad5bbf419b5b150a7be35189bf4f` | `e0a0efca87fd9debf63e64318d4fa9fc2d18e2b3`, BSD-3-Clause |
| base SDK | arbre upstream `v0.8.0` | `ff1c1b67f4dfae8f35269977bcd0570d1d174701` |

Le parent du fork et le tag upstream `v0.8.0` ont exactement le même arbre.
Le fork audité est donc un patch unique et mesurable de onze fichiers : audio
SDL, MnK, trois services XAM, runtime, UI et icône Linux. Les HEAD et le tag ont
été recoupés avec leurs remotes publics le 12 août 2026.

Le projet ne publie ni C++ généré ni actif jeu. Son manifeste indique seulement
le fichier `default.xex` et les versions générateur/SDK ; il ne scelle aucun
SHA-256, Media ID ou version XEX. Le SDK possède des tests PPC/unitaires, mais
ils sont désactivés par défaut et ses workflows construisent sans
`REXGLUE_BUILD_TESTS`. Aucun test ne couvre le patch Banjo.

Empreintes SHA-256 des sources principales :

- audio SDL : `38e89b74b308ab6eadf8896e81099305da1736a896aeab975f62a7578f1bc7a5` ;
- MnK : `b86d05e1cea529b70bad4c1885f4b8dce45702eab434800fab7d00dff483c959` ;
- XAM content : `974e613ea33565fd885875b3fb9e58a0a73909b94dc995f6f0c24124dd338a14` ;
- XAM UI : `623e4cbb0170164beeed0212b0e70d802d74f7b3b39b0b7d8b7f47fd7a54b49c` ;
- extracteur STFS : `75729b1af5fd03ea4902cd1e3eb33eb0354ccad1a95c18f860a68930bb69af63`.

## Audio `dummy`

Si `SDL_OpenAudioDeviceStream` échoue, le fork quitte le sous-système audio,
force le hint SDL à `dummy`, le réinitialise et réouvre le stream. Le callback
remplit du silence quand la queue est vide ; sinon il convertit la frame,
l'envoie au stream, recycle le buffer et libère le sémaphore invité. Cette
route explique le comportement AC6 déjà observé : sans backend dummy, le run
Xvfb peut s'arrêter après un seul `PRESENT` ; avec lui, les workers progressent.

Les limites sont explicites :

- le hint est global au processus et n'est pas restauré ;
- quitter tout le sous-système audio suppose qu'aucun autre client SDL n'est
  actif ;
- aucune mesure ne vérifie que le backend dummy rappelle exactement à la
  cadence console ou qu'il préserve les cues ;
- le callback publie le sémaphore après acceptation par SDL, pas après une
  présentation matérielle ;
- aucun test ne couvre échec initial, second échec, reconfiguration stéréo,
  sous-alimentation ou shutdown concurrent.

Décision AC6 : garder la variable d'environnement qualifiée pour le headless,
mais exclure cette route des mesures audio. Les gates ±1 dB et ±20 ms exigent
un périphérique/capture qualifié et une timeline M01 séparée.

## `GetKeystroke` et entrée

Le patch calcule les transitions de quatorze boutons dans
`MnkInputDriver::GetState`, puis empile un événement down/up. `GetKeystroke` ne
fait que dépiler. Le mécanisme débloque un titre qui appelle les deux APIs, mais
n'est pas une implémentation XAM générale :

- si le titre ne demande que `GetKeystroke`, aucun producteur ne recalcule les
  transitions ;
- triggers et axes n'émettent aucun événement ; `flags` est ignoré ;
- la queue n'a pas de plafond ;
- `packet_number_` est incrémenté à chaque `GetState`, même sans changement,
  contrairement au commentaire du header ;
- l'agrégateur retourne immédiatement `X_ERROR_EMPTY` du premier driver
  connecté. Avec un gamepad SDL connecté mais sans événement, il peut donc ne
  jamais consulter la queue MnK placée après lui.

Le replay AC6 poll-exact reste supérieur : il capture le retour réellement vu
au seam XAM, sans reconstruire a posteriori les arêtes d'un périphérique. Les
QoL clavier/souris ne seront projetées qu'avant l'état normalisé, jamais dans
le profil retail.

## XAM et UI

Le fork force `license_mask=1`, relâche une assertion `XamAlloc` et sélectionne
automatiquement le premier bouton dont le texte contient « offline ». Ces
choix sont utiles à un shell XBLA précis, mais divergent de toute sémantique
retail générale :

- une licence achetée ne peut pas être supposée sans identité/content licence ;
- l'argument inconnu de `XamAlloc` reste non qualifié ;
- le texte d'un bouton dépend de la langue et du contexte ; un autre dialogue
  contenant « offline » serait aussi auto-validé ;
- l'action utilisateur disparaît de la timeline replay.

AC6 reste hors ligne par scope, mais son frontend doit choisir explicitement la
route hors-ligne et enregistrer l'action normalisée. Aucun dialogue XAM ne sera
auto-validé par recherche textuelle.

## Extracteur STFS et paquetage

L'extracteur lit le container complet en mémoire, reconnaît seulement le magic,
parcourt une table plate et copie des blocs supposés contigus. Il annonce
lui-même ne vérifier ni signatures ni hashes et ne gérer aucun répertoire.
Les contrôles `start_block * 0x1000 <= file_size` n'intègrent pas le header ni
les tables de hash ; une lecture hors fin retourne silencieusement une tranche
plus courte, ensuite écrite comme succès. Il n'existe ni plafond de container,
ni vérification de longueur finale, ni détection de doublon, ni écriture
atomique.

Le launcher crée `.extracted_ok` après n'importe quelle extraction sans
manifeste d'entrées attendu ; même zéro fichier suffit. Ce marqueur autorise
ensuite le lancement. Le SHA-256 de `payload_version.txt` protège seulement les
binaires hôte embarqués, pas les octets retail extraits.

Cette approche est `divergent` pour AC6. L'import v2 conserve son index
SHA-256, ses écritures atomiques, ses blobs bornés et l'interdiction de relire
les PAC après import. Un éventuel lecteur STFS futur devra valider chaînes,
hashes, tailles, extents, doublons et identité du payload avant publication.

## Classement AC6

| Mécanisme | Classe | Décision |
|---|---|---|
| SDL dummy après absence de périphérique | `provisional-rexglue` | conserver seulement dans le harness headless |
| cadence/cues du backend dummy | `retail-needed` | mesurer ailleurs ; ne pas utiliser pour la parité audio |
| transitions `GetKeystroke` | `divergent` | replay XAM poll-exact et tests d'arêtes explicites |
| licence/offline automatiques | `divergent` | choix frontend et identité explicites |
| extracteur STFS | `divergent` | cache retail v2 reste autorité |
| code du projet racine | sans licence | aucun copier-coller |

La seule action immédiate est une garde documentaire déjà active : les runs
Xvfb gardent `SDL_AUDIODRIVER=dummy`, tandis que les captures audiovisuelles de
parité refusent ce backend. Aucun changement produit n'est requis.

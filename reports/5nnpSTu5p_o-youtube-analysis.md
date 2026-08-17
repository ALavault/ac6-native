# Analyse de la vidéo YouTube `5nnpSTu5p_o`

Observation effectuée le **15 août 2026** (Europe/Paris) avec `yt-dlp 2026.07.04`. Source : [page YouTube canonique](https://www.youtube.com/watch?v=5nnpSTu5p_o). Le JSON brut de l'extracteur est conservé dans [`5nnpSTu5p_o.metadata.json`](../5nnpSTu5p_o.metadata.json).

## Méthode et provenance

- **[YouTube]** : valeur exposée par YouTube et restituée par `yt-dlp -J --skip-download`.
- **[Calculé]** : conversion ou agrégation explicite d'une valeur extraite, ou mesure locale par `ffprobe`.
- **[Inféré]** : constat visuel sur la copie d'analyse allégée ; ce n'est pas une métadonnée YouTube.
- L'extraction initiale a signalé l'absence de runtime JavaScript configuré. Les formats listés sont donc ceux que l'extracteur a pu exposer et peuvent ne pas représenter toutes les variantes servies à d'autres clients YouTube.
- Aucun sous-titre YouTube n'étant disponible, l'analyse du contenu exigeait les images. La variante progressive **format 18**, 640×360, H.264/AAC, a été téléchargée dans `/fastdata/lavaulta/tmp` : 72 701 954 octets (69,33 MiB), soit la variante combinée la plus légère exposée. Elle n'est pas suivie dans le projet.

## Identité

| Champ | Valeur | Provenance |
|---|---|---|
| Titre | Ace Combat 6 Fires of Liberation Demo | YouTube |
| ID | `5nnpSTu5p_o` | YouTube |
| URL canonique | `https://www.youtube.com/watch?v=5nnpSTu5p_o` | YouTube |
| Chaîne / auteur | PartyCow2000 | YouTube |
| ID de chaîne | `UCtIHZrjiuuF5txgFE4vF7vw` | YouTube |
| URL de chaîne | `https://www.youtube.com/channel/UCtIHZrjiuuF5txgFE4vF7vw` | YouTube |
| Identifiant d'uploader | `@PartyCow2000` | YouTube |
| URL d'uploader | `https://www.youtube.com/@PartyCow2000` | YouTube |

## Publication

- **Date de publication [YouTube]** : 1er février 2021 (`20210201`). L'horodatage extrait est `1612202405`.
- **Durée [YouTube]** : 1 034 s, soit 17 min 14 s. **[Mesure locale]** La variante téléchargée dure 1 033,451 s ; cet écart inférieur à une seconde vient de la granularité/du conteneur.
- **Langue [YouTube]** : absente. **[Inféré]** Les menus, dialogues radio sous-titrés dans l'image et écrans promotionnels observés sont en anglais.
- **Catégorie [YouTube]** : Gaming.
- **Licence [YouTube]** : absente de la réponse ; aucune licence ne doit être déduite.
- **Statut [YouTube]** : publique, non diffusée en direct, restriction d'âge `0`.
- **Description [YouTube]** : « Ace Combat 6 Fires of Liberation Demo », joueur indiqué « MooMoo 24 », puis alerte scénarisée annonçant l'attaque de Gracemeria et l'ordre de décollage immédiat.
- **Tags [YouTube]** : 31 tags. Ils couvrent notamment Ace Combat 6, gameplay/walkthrough, démo, campagne, Xbox Live/Xbox One, aviation, guerre Emmeria–Estovakia et `no commentary`. Les tags sont déclaratifs et ne prouvent ni la plateforme capturée ni les conditions d'exécution.

## Compteurs au 15 août 2026

| Compteur | Valeur | Remarque |
|---|---:|---|
| Vues | 314 | YouTube ; valeur évolutive |
| J'aime | 7 | YouTube ; valeur évolutive |
| Commentaires | absent | Non exposé dans la réponse, donc pas assimilable à zéro |

## Chapitres, miniatures et sous-titres

- **Chapitres [YouTube]** : absents (`chapters: null`).
- **Miniatures [YouTube]** : 42 entrées/variantes JPEG et WebP sont exposées. Elles comprennent les familles `default`, `mqdefault`, `hqdefault`, `sddefault`, `hq720`, `maxresdefault`, ainsi que trois images automatiques numérotées. Les dimensions explicitement renseignées vont de 120×90 à 1 920×1 080 ; plusieurs variantes n'ont pas de dimensions déclarées. La meilleure entrée déclarée est `maxresdefault.webp`, mais ses dimensions ne sont pas renseignées dans le JSON ; `maxresdefault.jpg` est annoncé à 1 920×1 080.
- **Sous-titres manuels [YouTube]** : aucun.
- **Sous-titres automatiques [YouTube]** : aucun.
- **Transcription** : indisponible. La vidéo contient des dialogues radio et des légendes anglaises incrustées par le jeu, mais celles-ci ne constituent pas une piste de transcription YouTube. Aucun texte intégral n'a été inventé ni reconstitué par OCR.

## Formats exposés

Les débits sont ceux fournis par l'extracteur. Les tailles marquées « exacte » proviennent de `filesize`; les autres sont des estimations `filesize_approx`. Conversion en MiB calculée avec 1 MiB = 1 048 576 octets.

| ID | Nature | Résolution / FPS | HDR | Codec(s) | Débit | Taille |
|---|---|---|---|---|---:|---:|
| 160 | vidéo seule MP4 | 256×144 / 30 | SDR | H.264 `avc1.4d400c` | 90,759 kb/s vidéo | 11,18 MiB exacte |
| 134 | vidéo seule MP4 | 640×360 / 30 | SDR | H.264 `avc1.4d401e` | 434,457 kb/s vidéo | 53,52 MiB exacte |
| 136 | vidéo seule MP4 | 1 280×720 / 30 | SDR | H.264 `avc1.64001f` | 1 638,055 kb/s vidéo | 201,80 MiB exacte |
| 137 | vidéo seule MP4 | 1 920×1 080 / 30 | SDR | H.264 `avc1.640028` | 3 293,435 kb/s vidéo | 405,73 MiB exacte |
| 18 | vidéo+audio MP4 | 640×360 / 30 | SDR | H.264 `avc1.42001E` + AAC-LC `mp4a.40.2` | 562,808 kb/s total | 69,34 MiB estimée |
| 139 | audio M4A | audio seul | n/a | HE-AAC `mp4a.40.5` | 48,790 kb/s | 6,01 MiB exacte |
| 140 | audio M4A | audio seul | n/a | AAC-LC `mp4a.40.2` | 129,476 kb/s | 15,95 MiB exacte |
| 251 | audio WebM | audio seul | n/a | Opus | 123,970 kb/s | 15,27 MiB exacte |

Trois storyboards supplémentaires (`sb0` à `sb2`) sont exposés à 160×90, 80×45 et 48×27 ; ce ne sont ni des flux vidéo ni des flux audio. Aucun format HDR n'est exposé. **[Mesure locale]** Le format 18 effectivement reçu est H.264 Main 640×360 à 30000/1001 fps, BT.709, avec AAC-LC ~128,0 kb/s ; son débit conteneur est ~562,8 kb/s.

## Résumé factuel du contenu

**[Inféré par observation]** Il s'agit d'une capture sans commentaire ajouté de la démo anglophone d'**Ace Combat 6: Fires of Liberation**. Après les logos et le titre, le joueur sélectionne la difficulté normale et consulte l'écran de commandes. Une cinématique montre le décollage d'urgence d'appareils emmeriens depuis Gracemeria. Le joueur pilote ensuite un chasseur en vue arrière, au-dessus de la ville et de ses environs, avec HUD, radar, communications radio sous-titrées, tirs de missiles et engagements contre des cibles aériennes et terrestres. La séquence jouable s'achève sans écran de score détaillé observé : la vidéo passe à des cartons promotionnels du jeu puis au logo final.

Cette vidéo est utile comme référence visuelle et temporelle du frontend, du lancement de mission, du HUD et du rendu de la démo. Elle ne prouve pas à elle seule la région du binaire, son hash, la cadence interne de simulation, ni l'identité exacte du XEX.

## Chronologie du contenu

Horodatages **[calculés à partir du média]**, descriptions **[inférées par observation]** :

| Temps | Contenu observé |
|---:|---|
| 00:00–00:35 | Logos Bandai Namco/Namco, écrans de middleware et mentions légales. |
| 00:35–00:43 | Écran-titre *Ace Combat 6: Fires of Liberation*. |
| 00:43–00:53 | Sélection de difficulté ; `NORMAL` est choisi. |
| 00:53–01:09 | Schéma des commandes de la manette et chargement. |
| 01:09–02:05 | Cinématique en temps réel : base aérienne, préparation, roulage et décollage des chasseurs ; alerte et communications sous-titrées. |
| 02:05–02:25 | Formation aérienne et arrivée vers Gracemeria ; mise en place de la mission. |
| 02:25–04:00 | Début du contrôle joueur, découverte du HUD/radar et premiers engagements aériens. |
| 04:00–09:00 | Combat soutenu au-dessus de la ville : poursuites, verrouillages, missiles et communications des alliés. |
| 09:00–12:40 | Progression dans la bataille avec cibles aériennes et terrestres, changements d'altitude et de cap autour de Gracemeria. |
| 12:40–15:45 | Engagements plus intenses ; multiples contacts ennemis, explosions et messages de situation radio. |
| 15:45–16:20 | Dernière phase jouée et sortie de la zone de combat annoncée dans les communications. Aucun tableau de résultats détaillé n'est montré. |
| 16:20–17:14 | Cartons promotionnels : rendu des zones de guerre, ampleur des combats, Xbox LIVE, logo et message de sortie du jeu. |

## Limites et données évolutives

- Les vues et mentions « J'aime », la disponibilité, la description, les tags, miniatures et formats peuvent changer après l'observation.
- L'absence de commentaire, licence, langue, chapitre ou sous-titre signifie « non exposé par cette extraction », pas nécessairement une propriété immuable de la vidéo.
- La chronologie est une segmentation descriptive issue d'images échantillonnées et de vérifications autour des transitions ; elle n'est pas fournie par YouTube.
- Les tailles de flux peuvent varier selon le client, les signatures d'URL et une retranscodification future. Le format 18 téléchargé a été mesuré localement ; les autres n'ont pas été téléchargés.

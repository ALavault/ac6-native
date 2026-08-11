# Cycle 1522 — oracle isolé et frontière produit auditée

## Résultat

Un worktree oracle propre, détaché et distinct des deux checkouts expérimentaux
a été créé dans `.tools/ac6-recomp-oracle-dcd41b` au commit exact
`dcd41b7457fcac8242f8ef40de83d1719390d5af`. Les checkouts sales existants
n'ont été ni modifiés ni utilisés.

Le manifeste `analysis/oracle/ac6-recomp-dcd41b/manifest.json` fige :

- le dépôt et commit AC6_recomp ;
- la configuration `ac6recomp_config.toml` par SHA-256 ;
- le SDK intégré comme arbre vendored, avec son dernier commit vendeur
  `06d1f5785153cd57c0e6b289f587adca67859714` et l'arbre Git exact
  `741541d6035616dc406f7d74c2fe8f155913c77b` ;
- les versions CMake, C++, Python, Git et Vulkan du contexte qualifié ;
- le PAL `default.xex`, 7 483 392 octets, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde` ;
- le projet Ghidra canonique et le contrat de sonde borné, lui-même haché.

Le dépôt SDK upstream ne permet pas de rattacher l'arbre vendored à un commit
upstream conservé. Le manifeste enregistre explicitement cette limite et
utilise le couple commit vendeur/arbre, qui est vérifiable depuis le checkout,
au lieu d'inventer un pin upstream.

`normalize_ac6_recomp_trace.py` impose une trace JSON déterministe avec adresse
invitée 32 bits, tick monotone, entrées normalisées, état graphique et hashes
de sorties. Le raw versionné est uniquement un fixture de contrat ; le
manifeste reste à `capture_status=not-captured`, donc ce fixture ne ferme aucun
gate de parité.

`audit_ac6_product_boundary.py` inspecte les sources produit, les symboles et
dépendances ELF via `readelf`/`ldd`, les chaînes du binaire et le staging. Il
refuse RexGlue, `rex_*`, AC6_recomp, XenonRecomp, `generated/` et les chemins
`.tools`. L'audit du tarball refuse en plus XEX, PAC et artefacts/captures retail.
Xbox, XAM et XMA restent des domaines natifs légitimes et ne sont plus
confondus avec une dépendance à l'oracle.

## Validation

- Qualification complète du worktree et du XEX : passe, une sonde hachée.
- Normalisation répétée du fixture : sorties byte-identiques, SHA-256
  `8fcec9c70437b2ecf6d1db2fca42bba7d6935c6a61e6a8f0b4319f1a8b64661d`.
- Audit produit : 211 sources, un ELF et 79 fichiers de staging passent.
- CPack TGZ : 85 entrées, audit de paquet passe.
- CTest complet : 73/73 passent ; deux tests à ressource externe sont sautés
  selon leur contrat.
- Tests Python : 96/96 passent.

## Risque résiduel

Aucune capture AC6_recomp réelle n'est encore qualifiée. Avant d'en accepter
une, il faut instrumenter le worktree sans le salir (adaptateur externe ou
patch reproductible appliqué à un second worktree), fournir l'entrée bornée et
faire passer le même manifeste. Jusqu'alors, AC6_recomp ne peut fermer ni JV,
ni JP, ni JG.

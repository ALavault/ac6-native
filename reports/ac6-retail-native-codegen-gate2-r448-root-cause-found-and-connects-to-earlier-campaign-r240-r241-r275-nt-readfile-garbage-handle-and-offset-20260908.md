# AC6 retail NTSC-U/J — r448 — CAUSE RACINE TROUVÉE, et elle rejoint une investigation antérieure déjà documentée (r240/r241/r275) : le stub natif `NtReadFile` reçoit un « handle » qui est en réalité un pointeur `FILE_OBJECT` (`0x829xxxxx`) et un décalage égal au motif de remplissage mémoire non initialisée `0xFEFEFEFE` — refuse correctement de fabriquer un succès, la chaîne entière `sub_821D5F48`→`sub_821D6C20`→`SIGSEGV` en découle

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Nommé par r447 : décompiler `sub_823CFE08`, la fonction censée
contenir la cible réelle du dispatch (`0x823D035C`) qui produit
`0x13D`.

## Correction méthodologique en cours de cycle

La décompilation initiale de `sub_823CFE08` (bornée par le point de
coupure du codegen suivant, `0x823D0E00`) a donné un corps de
**deux instructions seulement** — la fonction réelle est bien plus
courte que la plage supposée. Redésassemblage à partir de l'adresse
cible exacte (`0x823D035C`) : Ghidra a immédiatement échoué à
désassembler des instructions PPC valides à cet endroit
(« Bad instruction data »), **mais a spontanément nommé
l'emplacement `NtReadFile`** — signe qu'il s'agit d'un slot de table
d'import du noyau XEX, pas de code PPC ordinaire. Ce désaccord avec
la lecture de r447 (« du vrai code invité, pas un stub ») a immédiatement
mené à vérifier l'outillage natif existant plutôt que d'insister sur
l'hypothèse précédente.

## Établi — `NtReadFile` est un stub natif déjà existant et déjà extensivement instrumenté

`tools/materialize_native_import_stubs.py` contient une implémentation
détaillée de `NtReadFile` (ligne ~1723), avec des commentaires de
cycles antérieurs de la campagne (bien avant r399, donc avant toute
cette chaîne de cette session) :

> r240/r241 : le chemin PAC échoue toujours avec `handle=0x829xxxxx`
> (un `FILE_OBJECT` image, pas un petit `HANDLE`) et
> `offset=0xfefefefe` (des données de pile non initialisées, pas 0).
>
> r275 : les producteurs du pipeline de lecture PAC. `0x82778EB0` =
> l'arène globale écrite par `sub_821D5F48` avant qu'elle n'alloue le
> tampon de file ; `0x8293B930`/`94C`/`938` = le cluster de pool lu
> par `sub_821CC508`.

**Ces adresses et ces noms de fonctions sont EXACTEMENT ceux que
cette session (r413-r447) a redécouverts indépendamment, par une
méthode complètement différente (capture gdb en direct plutôt que
lecture du stub).**

## Établi EN DIRECT — la variable d'environnement de trace déjà construite confirme tout

Lancement avec `AC6_NATIVE_IMPORT_TRACE=1` (variable déjà supportée
par le stub, jamais utilisée dans cette session avant ce cycle) :

```
[NtReadFile] r1=0x8feffa20 r3=0x82918a78 r4=0x0000012f r5=0x00000000
             r6=0x00000000 r7=0x829e36cc r8=0x00000000 r9=0x00040000
             r10=0x8feffa78 offset=4278124286
[NtReadFile] *r10 dump: 00000000 fefefefe
[NtReadFile] globals: arena=0x16f80000 pool_obj=0x00000000
             pool=0x173b0028 gate=0 initcnt=389742640
[NtReadFile] queue@0x829ddd80: r316=00000000 r320=00000001
             r324=00000003 r328=00000000 r332=00000000 r340=00040000
[NtReadFile] invalid handle=0x82918a78 offset=4278124286 len=262144
```

**Correspondance exacte avec r240/r241** : `handle=0x82918a78` — un
`0x829xxxxx`, la même classe d'adresse `FILE_OBJECT` déjà documentée,
PAS un petit entier `HANDLE`. `offset=4278124286` = `0xFEFEFEFE` en
hexadécimal — **exactement** le motif de remplissage de données non
initialisées déjà nommé par r241. Le stub refuse correctement de
fabriquer un succès (`STATUS_INVALID_HANDLE`, `0xC0000008`) plutôt
que de risquer de corrompre les données PAC — décision déjà prise et
documentée par un cycle antérieur, toujours correcte.

`queue@0x829ddd80` — **le même descripteur constant** que
`sub_821CC508`/`sub_821CC008` utilisent (établi par r443-r446 cette
session) — confirme sans ambiguïté que c'est bien LA MÊME opération
suivie tout au long de cette chaîne.

## Ce que ceci établit — la chaîne causale complète, de bout en bout

1. `sub_821D5F48` (bootstrap Xenos/renderer, r441) prépare une
   opération de lecture asynchrone via `sub_821CC008`/`sub_821CC508`
   contre le descripteur `0x829ddd80`.
2. Cette opération appelle en interne `NtReadFile` avec un **handle
   qui est en réalité un pointeur `FILE_OBJECT`** (`0x82918a78`) et
   un **décalage non initialisé** (`0xFEFEFEFE`) — un bug de
   préparation d'arguments quelque part en amont dans cette chaîne
   PPC recompilée (ou une donnée de contexte que cet environnement
   offline ne peuple pas correctement).
3. Le stub natif `NtReadFile` (déjà écrit, déjà correct dans son
   refus de deviner) retourne `STATUS_INVALID_HANDLE`.
4. Après 5 tentatives (r446), `sub_821CC508` abandonne, retourne -1.
5. `sub_821D5F48` échoue, ne construit jamais l'objet de rendu à
   `*0x82935D98` (r436-r441).
6. `sub_821D7DE0` journalise l'échec de façon non fatale (r442) et
   appelle `sub_821D6C20` quand même.
7. `sub_821D6C20` déréférence l'objet jamais construit, sans garde →
   `SIGSEGV` (r435-r436).

**Chaque maillon de cette chaîne est maintenant établi par une preuve
directe, pas une supposition — certains redécouverts cette session,
d'autres retrouvés dans une documentation antérieure déjà correcte.**

## Non établi

- **Pourquoi le handle/décalage sont incorrects** — reste la seule
  vraie question ouverte. Soit un bug de codegen dans la préparation
  des arguments PPC quelque part en amont (non localisé), soit une
  donnée de contexte que cet environnement offline ne peuple jamais
  (le pointeur `FILE_OBJECT` pourrait provenir d'une étape de montage
  de contenu/PAC non modélisée). r240/r241 avaient déjà noté que
  « le prochain sondage honnête nommera le véritable appelant » — les
  informations de pile d'appel (`backchain`) capturées ce cycle
  (`lr=0x00000000` à chaque niveau) ne l'ont PAS résolu : les adresses
  de retour capturées sont toutes nulles, signe que la convention
  d'appel (`PPC_CONFIG_SKIP_LR`, déjà noté dans le commentaire du
  stub) empêche cette technique particulière de nommer l'appelant
  directement.

## Décisions prises

- Corriger sa propre lecture erronée de r447 (« code invité réel,
  pas un stub ») dès que la preuve directe (le nom `NtReadFile` que
  Ghidra a spontanément assigné) l'a contredite — plutôt que de
  forcer la décompilation d'un site qui n'est pas du code PPC valide.
- Utiliser l'outillage de trace DÉJÀ CONSTRUIT (`AC6_NATIVE_IMPORT_TRACE`)
  plutôt que de reconstruire une capture gdb équivalente — un stub
  déjà écrit par des cycles antérieurs contenait exactement
  l'instrumentation nécessaire, il suffisait de l'activer.
- Ne pas tenter de corriger le bug ce cycle — la cause exacte du
  handle/décalage incorrects reste à déterminer (bug de codegen vs
  donnée de contexte manquante), et un correctif à l'aveugle serait
  exactement le type de correctif que r241 avait déjà refusé à raison
  (« fabriquer un succès corromprait les données PAC »).

## Gate

Aucune source de production éditée ce cycle (investigation en direct
uniquement ; le script de désassemblage/décompilation
`DecompileD03D5CDispatchTarget.java`, committé, n'a produit aucune
information utile au-delà de confirmer que la cible n'est pas du PPC
ordinaire — gardé pour référence future malgré son résultat négatif,
cohérent avec la discipline de ce dépôt de ne pas cacher les impasses).
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées ci-dessus, inchangées — aucune source de
production affectée)
`ctest` (racine et natif) inchangé depuis r447 (aucune source de
production modifiée).

## Named for r449

Identifier POURQUOI le handle passé à `NtReadFile` est un pointeur
`FILE_OBJECT` (`0x829xxxxx`) plutôt qu'un `HANDLE` réel, et pourquoi
le décalage est non initialisé (`0xFEFEFEFE`) — remonter la chaîne
d'appel PPC (pas via `lr`, inutilisable ici, mais via une lecture
statique du désassemblage de `sub_821CC508`/`sub_821D5F48` autour du
site d'appel réel à `NtReadFile`, déjà localisé cette session) pour
déterminer si c'est un bug de traduction (une valeur mal propagée
dans le codegen) ou une étape d'initialisation manquante spécifique à
cet environnement offline. Reste ouvert sinon : décision de
committage de l'arriéré `native_vulkan_backend.cpp`
(r433/r434/r438).

## Files

Nouveau (committé) : `scripts/DecompileD03D5CDispatchTarget.java` —
résultat négatif honnête (la cible n'est pas du PPC désassemblable),
gardé pour éviter de refaire la même tentative à l'identique dans un
cycle futur. Aucun artefact gitignoré nouveau sous `reports/`.

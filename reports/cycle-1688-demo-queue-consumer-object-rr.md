# Cycle 1688 — objet complet du consumer de file, rr read-only

## Résultat

La lecture big-endian de tout le buffer `[r3,r3+0x60)` à l’entrée
`0x820FEFA8` donne 24 mots nuls, de `+0x00` à `+0x5C`, dans neutral et
START. Les offsets `+0x54` et `+0x58`, absents de la sonde courte
précédente, sont donc eux aussi observés à zéro dans cette frontière.

Les deux routes du même build noinline arrivent au tick 252, thread 25, avec
`r3=0x2EEEBE90`, `r31=0x82386CC0` et `LR=0x820FFD8C`. La sonde lit la mémoire
via le `raw_base` passé au code généré et décode les mots en big-endian; elle
ne modifie ni contexte ni mémoire guest.

Cette observation ne donne pas de sémantique au buffer et ne prouve pas un
consumer frontbuffer. Elle ferme seulement la présence d’un payload non nul
dans cette plage et cette exécution.

## Identité et méthode

| élément | valeur |
|---|---|
| cible | `Default.xex` PAL démo, SHA-256 `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| architecture | Xenon big-endian / Xenos |
| runtime | `/fastdata/lavaulta/tmp/ac6-demo-atomic-rr-noinline.12070` |
| binaire | SHA-256 `ed44dba583e2aa78d081ca52796ddc0e52b7b775015db41ca1198814c95b2ecc` |
| rr | commit `7352eb807ed75e3b51be85fa6a27f121235dbfb0`, binaire SHA-256 `33fd6e3eade957f5b0e4c7e12ddb9f6ff54ce522103ad418f1b6d14737f454d6` |
| sonde finale | `recompilation/ace-combat-6-demo/tools/rr_queue_consumer_object.gdb` |
| sonde SHA-256 | `f0adfdd2b3452cc5ae82821a442fb30837121673d440039c219e796d0d4e8bb7` |

La sonde utilise les symboles runtime `active_bridge` et
`current_guest_thread_id` pour le tick/thread; une tentative antérieure qui
lisait des offsets TLS historiques a été rejetée après une erreur d’accès et
ne fait pas partie de la preuve.

## A/B observé

| route | trace events SHA-256 | log | log SHA-256 | appel | plage lue | non-zéro |
|---|---|---|---|---|---|---:|
| neutral | `77014c9464e034dd7a709b1f37512ba6abca757b83724aecea6cf9e35c153fef` | `/fastdata/lavaulta/tmp/ac6-neutral-queue-consumer-object-gdb.log` | `da3677ab0264b6d1a54cd2d3cddf4fb1e9add57c4a5e82456eaa1991b80df25f` | tick 252/thread 25, `r3=0x2EEEBE90`, `r31=0x82386CC0`, `LR=0x820FFD8C` | `+0x00..+0x5C` (24×u32 BE) | 0 |
| START | `a11313387e8704ed1ca15728585f128736caffe5402dcb7da8eedc44ad73df9a` | `/fastdata/lavaulta/tmp/ac6-start-queue-consumer-object-gdb.log` | `ba29f33ff532459605095e41342789d0c9b57486aade885c7f9ecb0a9d7bec67` | tick 252/thread 25, `r3=0x2EEEBE90`, `r31=0x82386CC0`, `LR=0x820FFD8C` | `+0x00..+0x5C` (24×u32 BE) | 0 |

Les valeurs host `$pc` diffèrent par ASLR et ne sont pas promues comme PC
guest. La fin du replay affiche `SIGKILL` à `syscall_traced`, comme les
replays précédents; ce signal technique n’est pas une sortie guest propre.

## Jointure avec le branchement précédent

Le champ `r31+0x40` est nul dans les deux captures et le cycle 1687 n’a trouvé
aucun hit sur `0x820FEA88`, `0x8226D6A0` ou `0x8226E398`. Le résultat courant
étend seulement la mesure à tous les mots du buffer; il ne transforme pas le
chemin par défaut en preuve de rôle.

Le cross-match littéral de `sub_820FFCA0` montre la construction qui précède
l’appel : `r11 = r31 + r10*96 + 208`, quatre chargements vectoriels aux
offsets `0x00/0x10/0x20/0x30`, puis copie des mots source `+0x40..+0x58`
vers la pile `r1+0x90..r1+0xA8`; l’objet passé est ensuite `r1+0x50`.
Cette séquence est une provenance de bytes/contrôle de flot, pas un nom de
ressource. Les 24 mots lus dans l’objet après cette construction sont tous
nuls dans les deux traces.

## Qualification

- `demo-qualified` : identité PAL/runtime, ABI de contexte, lecture bornée
  big-endian, A/B même build et zéro mot non nul sur `+0x00..+0x5C`.
- `demo-observed` : objet de pile non nul à l’entrée, mais contenu nul au
  tick 252 dans les deux routes.
- `xenia-generic` : aucun élément.
- `unknown` : producteur sémantique du buffer, rôle de la file, consumer
  frontbuffer, pixels, frontend, mission et résultat.

## Garde et prochain test

Conserver l’ABI `PPCContext` (`r3=0x00`, `r1=0x10`, `r4..r31=0x20+(reg-4)*8`,
`lr=0x100`), la plage `[r3,r3+0x60)` et la limite de slots
`[0x82386D90,0x82386E30)`. Le prochain checkpoint est la jointure des
loads/stores qui construisent ce buffer et des slots de file, dans une seule
fenêtre neutral/START bornée; toute valeur non nulle ou divergence doit
arrêter le corridor. Aucun état synthétique, retail, actif propriétaire ou
mutation Xenia/Ghidra/C++ généré/microcode n’est admis.

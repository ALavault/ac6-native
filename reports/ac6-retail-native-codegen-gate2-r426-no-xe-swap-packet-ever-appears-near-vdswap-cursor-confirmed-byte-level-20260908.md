# AC6 retail NTSC-U/J — r426 — confirmé au niveau octet : aucun paquet PM4 `XE_SWAP` n'apparaît jamais autour du curseur que `VdSwap` transmet, sur les 5 appels capturés — de vrais paquets PM4 existent (opcodes 0x36/0x46/0x3c), juste jamais 0x64

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`.
Aucun oracle utilisé.

## Contexte

Suite de r425 (nommé pour r426) : lire `sub_821F03B0` (l'appelant
direct de `VdSwap`) pour localiser où/si le jeu construit un paquet
PM4 `XE_SWAP` avant d'appeler ce noyau.

## Correction méthodologique — même piège que r412, retombé dedans puis corrigé

Première tentative de capture : lecture de `ctx.r3` (le curseur passé
à `VdSwap`) via l'offset `ctx+0x08` (convention validée ailleurs dans
cette investigation) — a produit des adresses de PILE
(`0x8feff9b0`-range), en contradiction directe avec les traces
`AC6_NATIVE_VD_TRACE` déjà établies (`secondary=0x12740b78`-style,
r418/r425). Désassemblage statique de `__imp__VdSwap`
(`gdb disassemble`) : `mov (%r14),%edx` — **`ctx.r3` vit à `ctx+0x00`
pour cette fonction précise, pas `ctx+0x08`** — exactement le même
piège déjà documenté par r412 pour une fonction différente. Corrigé
avant de publier quoi que ce soit basé sur la première lecture.

## Établi

### Le curseur, correctement lu, pointe vers un vrai flux PM4 structuré — sans jamais de paquet `XE_SWAP`

`r426_swap_buffer_dump.gdb` (`ctx.r3` lu à `ctx+0x00`, fenêtre de 264
octets autour du curseur, aux 5 appels `VdSwap` de la fenêtre de sonde
`25000ms`) : les adresses de curseur retrouvées (`0x125c068c`,
`0x12640xxx`, `0x126c0xxx`, `0x12740xxx`, `0x127c0xxx`) **correspondent
exactement** aux valeurs `secondary=...` déjà vues dans les traces
`AC6_NATIVE_VD_TRACE` (r418/r425) — confirme que cette lecture est
maintenant correcte. Le contenu autour de ces curseurs est un flux PM4
authentique : en-têtes de type 3 reconnaissables
(`0xC0003600`, `0xC0004600`, `0xC0043C00` — décodage `header = 0xC0000000
| ((count-1)<<16) | (opcode<<8)` donne opcodes `0x36`, `0x46`
[`kOpcodeInvalidateStateExtended`, déjà nommé dans
`native_xenos.h`], `0x3C`).

**Recherche systématique de l'en-tête `XE_SWAP` (`opcode=0x64`,
motif `0x0000XX6400` selon le compte) et de la signature `SWAP`
(`0x53574150`) sur les 5×264 octets capturés : ZÉRO occurrence.**

## Ce que ceci établit

**Confirmation au niveau octet de r425**, avec une marge d'erreur
bien plus faible qu'un comptage de paquets décodés : le tampon de
commandes que le jeu construit, exactement là où il indique à
`VdSwap` que se trouve la fin de son travail le plus récent, contient
de VRAIS paquets PM4 d'état/rendu — mais jamais, sur aucun des 5
appels capturés, le paquet `XE_SWAP` qui déclencherait une
présentation. Ce n'est ni un problème de décodage (r425 l'avait déjà
écarté au niveau des lots), ni un problème de fenêtre de capture trop
étroite (264 octets couvre largement la fenêtre de 48 octets du seul
commit propre confirmé par r425). **Le jeu invité, dans cette fenêtre
de sonde, ne construit jamais lui-même de paquet `XE_SWAP` — il
appelle `VdSwap` (le noyau de présentation) sans jamais avoir préparé
la commande de présentation elle-même.**

### Une fenêtre de sonde 3× plus longue (75s) ne change rien

`AC6_NATIVE_VD_TRACE=1`, `AC6_NATIVE_PROBE_WINDOW_MS=75000` : **10
lots drainés, `present=0` dans chacun** — toujours zéro. Plus
révélateur : la dernière activité de l'anneau (`vd publish
write=79 read=73`) plafonne au même niveau que ce qu'une fenêtre de
`25s` atteignait déjà (r425 : `write_index` grimpait jusqu'à `73` en
25s) — **l'anneau n'avance plus du tout au-delà de ce point, même
avec trois fois plus de temps réel accordé.** Ceci écarte directement
la première branche du fork nommé pour r427 (« le jeu n'a simplement
pas encore atteint son premier swap réel, donner plus de temps
suffirait ») : le jeu ne progresse pas vers un swap avec plus de
temps, il PLAFONNE tôt et reste bloqué dans le même état.

## Non établi

- **Pourquoi** — non lu : le code qui DEVRAIT écrire ce paquet
  (probablement plus haut dans la pile que `sub_821F03B0`, ou
  conditionné par un état de jeu/scène qui n'est jamais atteint dans
  cette fenêtre de 25s) n'a pas été localisé. `sub_821F03B0`
  lui-même (1015 lignes) n'a été lu qu'en STRUCTURE (recherche de
  motifs), pas ligne par ligne.
- **Si une fenêtre de sonde plus longue** (au-delà de 25s) ferait
  éventuellement apparaître un paquet `XE_SWAP` — non testé.
- Aucun correctif proposé ni appliqué ce cycle.

## Décisions prises

- Corriger la lecture de `ctx.r3` avant de publier, dès que le
  désassemblage statique a contredit la première capture — même
  discipline que r412 sur elle-même (précédent CLAUDE.md : corriger
  soi-même, par nom et numéro de cycle).
- Ne pas lire `sub_821F03B0` ligne par ligne sans savoir d'abord SI le
  paquet est écrit ailleurs — la preuve octet-par-octet de ce cycle
  rend cette lecture nécessaire mais oriente déjà la recherche vers
  "plus haut dans la pile" ou "jamais dans cette fenêtre" plutôt que
  "mal formé dans `sub_821F03B0`".

## Gate

Aucune source de production éditée ce cycle.
```
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json
```
(sorties consignées dans le commit de ce cycle)
`ctest` relancé en entier.

## Named for r427

Le fork « donner plus de temps » est déjà écarté par ce cycle
(75s testé, plafond identique à 25s). Reste : lire `sub_821F03B0`
ligne par ligne (et remonter `sub_8234F558`/`sub_8233E0A8`/
`sub_8233B5A0` si nécessaire) pour localiser soit la condition qui
gate l'émission du paquet `XE_SWAP`, soit — piste également ouverte
par le plafonnement observé — ce qui bloque le jeu tôt et l'empêche
de progresser vers son premier swap réel du tout, dans la même veine
que r399-r422 mais pour un sous-système différent (rendu, pas
allocateur/threads).

## Files

`recompilation/ace-combat-6-retail/artifacts/retail-us-native-r404-bucket17-writer/r426_swap_buffer_dump.gdb/.log`.

# Cycle 1638 — provenance `rr` de la file de rendu démo

## Résultat

Une trace `rr` locale, rejouée avec le binaire épinglé, confirme les
frontières primaire `0x820FF8D8` et worker `0x820FFCA0` autour de la file guest
`0x82386CC0`. Les indices sont lus en big-endian aux adresses
`0x8238CD90` (producteur) et `0x8238CD94` (consommateur), sans mutation du
runtime.

| champ | observation `demo-qualified` |
|---|---|
| worker | `sub_820FFCA0`, tick 221, thread 25, producteur=0, consommateur=0 |
| primaire | `sub_820FF8D8`, ticks 252–299, 48 appels, thread 1, producteur=0, consommateur=0 |
| transition producteur worker | `1 → 0`, tick 299, thread 25, dans `sub_820FFCA0`, retour hôte `0x…e219`, callsite `0x…e214` |
| transition consommateur worker | `1 → 0`, tick 298, thread 25, dans `sub_820FFCA0`, retour hôte `0x…e259`, callsite `0x…e254` |
| transition producteur helper | `0 → 1`, tick 299, thread 1, retour hôte `0x…96ca`, callsite `0x…96c5` |

La pile hôte des deux transitions passe par
`GuestMemory::store_u32 → AC6_PPC_STORE_U32 → sub_820FFCA0 →
sub_820FF700 → sub_822EE158 → sub_821A93F8 → GuestBridge::execute_guest_thread`.
Les valeurs hôte `0x01000000` ont été reconverties en valeur guest `1`. Les
messages `Old/New` de `rr` en reverse sont inversés par rapport à l’exécution
forward; les transitions ci-dessus sont normalisées dans le sens guest.
Les retours hôte des frames générées joignent les callsites des stores de
reset aux mappings guest `0x820FFD94` et `0x820FFD98`; le LR guest réel reste
inconnu.

## Mappings guest et inconnues

Les bytes PAL qualifiés indiquent, dans les fonctions exécutées, les candidats
suivants :

| champ | PC candidat | bytes | instruction |
|---|---:|---|---|
| producteur | `0x820FFD94` | `93 7F 60 D0` | `stw r27,0x60D0(r31)` |
| consommateur reset observé | `0x820FFD98` | `93 7F 60 D4` | `stw r27,0x60D4(r31)` |
| consommateur chemin normal | `0x820FFD78` | `91 5F 60 D4` | `stw r10,0x60D4(r31)` |
| helper producteur | `0x820FF75C` | `91 7F 60 D0` | `stw r11,0x60D0(r31)` |
| lectures worker | `0x820FFCE4/0x820FFCE8` | `81 7F 60 D0` / `81 5F 60 D4` | `lwz` producteur / consommateur |

Les stores de reset sont `demo-qualified` par retour/callsite hôte et bytes
PAL; le chemin normal `0x820FFD78` reste `static-candidate` et n’est pas le
store de la transition observée. Le champ `ctx.lr=0x820FFCE4` est un marqueur
de contrôle généré, pas un LR guest promu. Le payload de slot et la
sémantique menu restent inconnus.

## A/B et provenance

- cible : `Default.xex`, SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` ;
- bytes PAL recoupés : basefile SHA-256
  `b98a9ac1f5a2da4c0b6e3bbae1d6cf7fe8c1fc2292b1cef51cc627581aa14218` ;
- `rr` : `.tools/rr-install/bin/rr`, commit
  `7352eb807ed75e3b51be85fa6a27f121235dbfb0`, binaire SHA-256
  `33fd6e3eade957f5b0e4c7e12ddb9f6ff54ce522103ad418f1b6d14737f454d6` ;
- script de frontière SHA-256
  `5c8a49c9a5162b0923110e727a585dd5a3b22e855c27c170184cc3a91c91a1de` ;
- log de frontière SHA-256
  `e57793ae8bdf3a308c9cfd06d8a68f69276ce2f02550a5c5e54d82eff4e46ac6` ;
- log inverse SHA-256
  `cdb1f792d5ba2b54d620d9176b46f7b9961ef87afa63fae4f881971a77b86c1a` ;
- log forward producteur SHA-256
  `f3af53f6d4717f4e5096054be7dba493c493273cc254ecd5242d41868391585e` ;
- A/B direct/`rr` depuis stores neufs : RTPLY
  `0b5ffbbdb76341a42461d69749624b193abcda740b4da699d98afb2a82785182`,
  rapport `e3f6b8e2ed4a873481be34eecda2986b1bc11eb55a3d878f9844179eef59785a`,
  163 `PRESENT`, frontend/mission/terminal faux.

La capsule durable est
[`analysis/demo/ac6-demo-start-queue-rr-provenance-v1.json`](../analysis/demo/ac6-demo-start-queue-rr-provenance-v1.json).

## Prochain checkpoint

Sur une nouvelle trace autorisée, relever le LR guest réel et le contenu du
slot de 96 octets aux callsites `0x820FFD94/0x820FFD98`, puis refaire un A/B
frais. Tant que le payload consommé et sa sémantique ne sont pas qualifiés,
la file reste une frontière de scheduler observée et le renderer demeure
fail-closed.

## Politique

Aucune preuve retail n’est fusionnée. Xenia/ReXGlue, Ghidra, C++ généré,
microcodes et actifs propriétaires ne sont ni modifiés ni suivis.

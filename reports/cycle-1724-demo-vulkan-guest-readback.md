# Cycle 1724 — readback Vulkan guest-owned du frame neutre

## Verdict

Après la fermeture structurelle PM4, deux probes codegen-ON fraîches ont été
exécutées sous `SDL_AUDIODRIVER=dummy xvfb-run -a` avec le backend Vulkan : une
neutral, puis une START au tick 252. Les deux routes atteignent le même flux
graphique, écrivent la destination exacte du `XE_SWAP` dans la mémoire guest,
la relisent, la dé-tilent et vérifient le SHA linéaire contre le résultat du
resolve.

Le readback guest-owned est donc reproductible pour le frame atteint, mais il
est entièrement noir (`0c660f2b…a4913a5f`). Ce n’est ni une screencap ni la
preuve d’un frontend : le source EDRAM PAL reste nul dans cette exécution et
la lane visuelle non noire demeure ouverte.

## Identité et commandes

| élément | valeur |
|---|---|
| cible | `Default.xex`, démo PAL, Xenon BE/Xenos |
| XEX SHA-256 | `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8` |
| binaire codegen-ON | `a4a5b57f2aaf7404fe2a84f8f7ef34a974b1304c14f81de8021271d183957811` |
| backend | Vulkan 1.1 sous Xvfb, `SDL_AUDIODRIVER=dummy` |
| bornage | `probe --until frontend --max-ticks 253` |
| capsule | `analysis/demo/ac6-demo-vulkan-guest-readback-v1.json` |

Le shader resolve générique ReXGlue reste utilisé uniquement comme autorité
générique : commit `cb58065c…d8f`, SPIR-V SHA
`e8cfb0d…87b32`, validation `spirv-val` déjà PASS. Aucun microcode, SPIR-V ou
sortie générée n’est suivi dans le projet.

## Résultats neutral/START

| statistique | neutral | START |
|---|---:|---:|
| retour `probe` | 4 (`max_ticks`) | 4 (`max_ticks`) |
| ticks | 253 | 253 |
| shaders | 5 | 5 |
| draws | 26 | 26 |
| presents | 1 | 1 |
| modules Vulkan | 4 | 4 |
| pipelines graphics | 2 | 2 |
| normal draw | 1 | 1 |
| neutral resolve | 1 | 1 |
| guest writeback | oui | oui |
| SHA normal 640×360 | `0b150fd3…ec58366` | identique |
| SHA resolve linéaire 1280×720 | `0c660f2b…a4913a5f` | identique |

Les lignes renderer et les sections `graphics` des rapports sont identiques;
les traces globales diffèrent seulement par l’événement START injecté. Les
stderr sont vides et byte-identiques.

## Jointure guest-owned

Le commit vérifié est conditionné par les champs PM4 déjà observés :

- destination `0x1374A000`;
- format `6`, tiled, 1280×720;
- étendue tiled `0x398000` octets, linéaire `0x384000` octets;
- padding guest conservé;
- relecture guest puis untile avant comparaison SHA.

Le résultat linéaire est entièrement zéro. Cela qualifie la chaîne d’adresse,
de pitch/format/tiling, de resolve et de relecture pour ce frame précis; cela
ne qualifie pas la source EDRAM comme rendu de menu ou de mission.

## Classification et garde

- **demo-qualified** : readback guest-owned exact et reproductible, tuple
  destination/format/tiling/dimensions, neutral/START A/B;
- **demo-observed** : deux probes Vulkan fraîches et leurs digests;
- **xenia-generic** : SPIR-V resolve et oracle de tiling;
- **unknown** : pixels non noirs, frontend, XMA, audio et mission.

Aucune screencap n’est produite, aucun fallback `play` n’est activé et la
production reste fail-closed. Le prochain checkpoint doit fournir une source
EDRAM PAL non nulle qualifiée, ou conserver explicitement l’état noir sans
inventer un frontend.

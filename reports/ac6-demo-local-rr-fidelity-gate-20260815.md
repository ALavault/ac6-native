# Gate de fidélité `rr` local — démo PAL AC6

## Portée

- Cible : `ac6-demo-xbox360-pal`, `Default.xex` SHA-256
  `de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`.
- Route : neutral, backend headless, 253 ticks, sans HID.
- `rr` local : `.tools/rr-install/bin/rr`, source
  `7352eb807ed75e3b51be85fa6a27f121235dbfb0`, binaire SHA-256
  `33fd6e3eade957f5b0e4c7e12ddb9f6ff54ce522103ad418f1b6d14737f454d6`.
- Runtime codegen-ON temporaire SHA-256
  `24712c2487c94f917edd7635dfd6abb082477888b2c5a4a25e9ebc7d03920bd8`.

## Résultats

Le replay RTPLY direct, son enregistrement `rr` et deux replays autopilot
terminent tous avec `deterministic=true`, 763 événements et le même stdout
SHA-256 `823c34552a2d8a8af1e7a83c7eea02568ecbdb3f51088f44a5afc38a8c40a9bd`.
Le RTPLY d'entrée et les deux RTPLY produits par les probes ont le même SHA-256
`1d41d2e26003a631f8bec19534812a258ffc5d90466be885f6f7f3df797ebef7`.

Les probes direct et sous `rr` terminent tous deux au frontier attendu
`max_ticks`, tick 253, et produisent des rapports byte-identiques SHA-256
`d96a9b68a90f389207ee725ea5f182ea935d798f15c341bc455c6208370b6a79`.
Ils scellent donc les mêmes preuves graphiques :

| Champ | Valeur |
|---|---:|
| Ring | `0x126CA000`, 131072 dwords |
| RPTR / WPTR | 7 / 25 |
| Soumissions / dwords | 2 / 25 |
| IB intermédiaire | 11 dwords, `ef7ab6e4832aed218b50126464de899ccf0f4bf2eaf26ecfac6371c51671d2b0` |
| IB principal | 3029 dwords, `d121c8d8cf55bcb755fa558c4d54a9311f4520fa2e8bb5e34b25920f107358d6` |
| `VD_SWAP` | tick 252, 1280x720, adresse `0x1374A000`, format brut 6 |

Les codes retour 4 des probes signifient le terminal contrôlé `max_ticks`, pas
un échec de fidélité. Le stdout des deux probes diffère uniquement parce qu'il
imprime le chemin distinct du rapport ; les rapports et traces sont identiques.

## Conclusion et limites

Le `rr` local est qualifié pour le reverse-debug CPU de cette route neutral
headless bornée : aucune dérive de scheduler, RTPLY, PM4, IB ou swap n'est
observée. Cette preuve ne qualifie pas encore un run Vulkan, une route START,
un timing HID, ni une généralisation à un autre frontier. Chacun exige son
propre A/B avant promotion.

Les traces locales restent sous
`/fastdata/lavaulta/tmp/ac6-demo-rr-gate.jLnJ38/` et ne sont pas suivies ; elles
occupent environ 919 Mio et peuvent servir au prochain watchpoint inverse.

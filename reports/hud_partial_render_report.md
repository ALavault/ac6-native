# HUD/textes/chiffres — qualification partielle

Périmètre : chaîne HUD symbole/texte/valeur numérique et override PAL `atoi`
`0x82382480`. Sources conservées par provenance : `bridge` (Ghidra canonique),
`observe` (logs cycle 454/mesures de draws), `stock` (contrat PAL/XEX).

## Résultat

- Les panneaux/boutons sont construits, soumis et dessinés : 4-vertex quads
  visibles (cycle 361).
- Les libellés/lignes sont construits et soumis, mais absents au niveau pixels :
  lots 8/20/64 vertices, dont 64 vertices = 16 glyphes (cycle 361). Le lot
  `028B7000` est le dernier draw (cycle 373) : l’occlusion ultérieure est donc
  réfutée pour ce lot.
- La planche/atlas de glyphes est la frontière restante démontrée, mais son bind
  exact, son descripteur et son contenu ne sont pas prouvés : `unknown`.
- `M70000_222` atteint la préparation et les stages, mais ses lookups
  `0x822C1B48`/`0x8228B6D8` retournent 0. Cela qualifie un échec de résolution
  symbole observé, sans distinguer stock absent, bridge incomplet ou suppression
  appelante.
- `LOAD_W_003` résout à 69 et parcourt les mêmes stages. Les frontières
  `0x820F8924`, `0x820F8990`, `0x8228CA14`, `0x8228CA34`, `0x8228CA6C` sont donc
  observées exécutées dans ce parcours. `0x820F885C` reste `unknown` faute de
  preuve ciblée existante.
- L’override PAL `0x82382480` est qualifié par bridge+stock : thunk
  `li r5,10; li r4,0; b 0x823821D0`. Le défaut historique (`"200"` -> 22) est
  corrigé par conversion décimale bornée; cycle 548 rapporte 7 tests passants.

## Frontières et binds

Les traces prouvent des étapes de chaîne et des lots soumis, pas un bind atlas
valide ni l’écriture de l’image glyphes. Les nombreux `NULL VIEW` du log cycle
454 ne sont pas attribués à l’atlas sans association base/draw démontrée.
Conclusion : `draw_absent` pour les glyphes, `atlas` comme cause de chemin
qualifiée, et `unknown` pour le bind exact/pixels masqués.

Références : `reports/cycle-361-text-runs-identified.md`,
`reports/cycle-373-draw-order.md`, `reports/cycle-548-atoi-fix-and-ghidra-bridge-reconciliation.md`,
`reports/logs/cycle-454-oracle-save-rooted-visual/ac6recomp.log`.

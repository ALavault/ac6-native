# Prompt compact — prochain checkpoint main AC6

```text
Tu es le main thread AC6. Cible exclusive : démo PAL Default.xex,
SHA-256 de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8,
Xenon big-endian/Xenos. Les cycles 1715/1716 qualifient seulement l’A/B
direct/rr de l’expérience XMA opt-in et l’arithmétique PAL; le registre et
l’effet matériel restent unknown.

Checkpoint unique, read-only : sur des exécutions neutral et START fraîches,
capturer avec PC/LR/thread/tick :
1) la valeur lue à 0x7FEA1800 dans FUN_82356510 ;
2) le store vers le global 0x829DA52C ;
3) P/G/I/A/V juste avant le stwbrx de 0x823572D8, puis eieio 0x823572DC.
Utilise uniquement .tools/rr-install/bin/rr commit
7352eb807ed75e3b51be85fa6a27f121235dbfb0, process frais, sans resynchroniser
ni attribuer de sémantique depuis Xenia/ReXGlue.

Scelle l’équation PAL observée : I=((P-G)>>6)&0xffff,
A=0x7FEA1A80+((I>>5)<<2), V=1<<(I&0x1f). Compare neutral/START et direct/rr.
Publie un reçu/capsule dans reports/ et analysis/demo/ avec hashes, identité
XEX/basefile/binaire/rr, et classification demo-qualified/demo-observed/
xenia-generic/unknown. Si la jointure P/G/A/V échoue, ne mappe aucun MMIO,
conserve le trap ordinal 548 et désactive l’expérience hors production.

Ne modifie ni Ghidra, ni Xenia/ReXGlue, ni C++ généré, ni microcodes; aucun
retail, audio, readback, screencap ou actif propriétaire. Termine par CTest
codegen ON/OFF et mets à jour STATUS/handoff seulement avec les hashes vérifiés.
Références : reports/cycle-1715-demo-xma-rr-ab.md,
reports/cycle-1716-demo-xma-address-join.md,
analysis/demo/ac6-demo-xma-rr-ab-v1.json,
analysis/demo/ac6-demo-xma-address-join-v1.json.
```

## État au moment de l’émission

- `rr` local : commit `7352eb807ed75e3b51be85fa6a27f121235dbfb0`, binaire
  SHA-256 `33fd6e3eade957f5b0e4c7e12ddb9f6ff54ce522103ad418f1b6d14737f454d6`.
- Frontier opt-in neutral/START : tick 1048, thread 21, LR `0x823572AC`,
  tentative `0x7FEA1A80`, trap avant effet, 911 PRESENT.
- Validation récente : CTest codegen-ON `17/17`.
- Aucune screencap, audio ou sémantique MMIO n’est qualifiée.

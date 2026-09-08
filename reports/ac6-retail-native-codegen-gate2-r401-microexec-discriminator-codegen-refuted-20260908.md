# AC6 retail NTSC-U/J — r401 — microexec discriminator: codegen mistranslation refuted for the r399 double-issue, cause is upstream heap state

**Qualification** : NTSC-U/J retail, `default.xex` SHA-256
`6eefba42cdfe9121207e534d8d290009c98b1a8c60ae5334a33a4f15167cbbbc`
(`ghidra-projects/ac6-us`), ISO SHA-256
`204c5e645d79da8776699c12f17bd069f869fbdb10ae79015d1a0ef2b743c98c`.
Aucun oracle utilisé.

## Contexte

Suite du plan approuvé (r400) : avant un dix-septième correctif ad hoc à
l'allocateur, trancher codegen-vs-état-amont en un cycle avec le harnais
`MicroExecuteFunction.java` déjà existant.

## Établi

Capturé en direct (gdb) l'état exact (`ctx.r3=0x10000000`, `r4=0`, `r5=1`
puis `r5=256`, même `sp=0x8feffac0`) et un instantané mémoire du tas
invité `[0x10000000, 0x10200000)` au moment précis des deux appels
`sub_821F9E10` qui, dans le run natif compilé, retournent tous deux
`0x10082ab0` (re-confirmé, identique à r398). Rejoué ces deux états dans
`MicroExecuteFunction.java` (interprète p-code indépendant de Ghidra sur
les MÊMES octets d'instruction retail) : **les deux cas retournent
également `r3=0x10082ab0`**, `exit=return` propre (pas de fault),
`steps=212`/`127`.

**Deux moteurs d'exécution indépendants (le C++ compilé par XenonRecomp,
et l'interprète p-code de Ghidra) s'accordent sur la même sortie, à
partir du même état capturé.** Une mauvaise traduction de codegen
n'aurait raisonnablement pas dû survivre à une réimplémentation
indépendante des mêmes instructions. **Ceci réfute une mauvaise
traduction de codegen dans `sub_821F9E10` comme cause du double-octroi.**
La fonction fait ce que ses instructions disent de faire ; le problème
est en amont — dans l'état du tas au moment de ces deux appels, pas dans
la traduction de la fonction elle-même.

Nécessité de toucher `scripts/MicroExecuteFunction.java` (son
`QUALIFIED_XEX_SHA256` était figé sur le seul hash PAL de la suite de
calibration) : élargi en `Set<String>` incluant le hash NTSC-U/J,
**prouvé sans effet sur le chemin PAL existant par comparaison directe
avant/après** (sortie JSON identique octet pour octet sur
`rotation-822a1e80.spec`). La calibration automatisée complète
(`audit_microexec_harness_calibration.py --check`) n'a pas pu tourner :
bloquée par une lacune préexistante et sans rapport (charge utile
extraite manquante, gitignorée, absente de ce bac à sable) — nommée pour
r402, pas résolue ici. Un second constat préexistant et sans rapport a
aussi été trouvé : `rotation-822a1e80.ppc.json` (l'instantané de
référence committé) ne correspond déjà plus à une exécution fraîche de sa
propre spec, indépendamment de tout changement de ce cycle — également
nommé pour r402.

## Non établi

- Quel mécanisme en amont est responsable (initialisation invité, appel
  d'allocation antérieur non tracé, ou stub hôte — `MmAllocatePhysicalMemoryEx`
  de r389 reste le suspect nommé, jamais suivi). Le tas capturé
  (`heap2_call1_size1.bin`) est maintenant figé et rejouable pour la
  suite de la chasse à l'écrivain (r378-r398), au lieu d'une observation
  uniquement live.
- Si le retail lui-même atteindrait jamais cette forme de tas — question
  d'oracle (trace Xenia), décision de budget de l'utilisateur, pas
  tranchée ici.
- Recalibration complète du harnais microexec (voir ci-dessus).

## Décisions prises

- Élargir le gate SHA du harnais (changement additif, scope minimal)
  plutôt que le remplacer ou créer un second script dupliqué — vérifié
  par comparaison directe A/B plutôt que supposé sûr.
- Ne pas tenter de reconstruire la charge utile extraite manquante
  (`reports/logs/cycle-739-...`) ce cycle — hors périmètre du blocage
  r399, nommé pour r402 à la place.

## Gate

```
ctest (recompilation/ace-combat-6-retail) inchangé -- aucune source de production éditée ce cycle
python3 tools/audit_ac6_mission01_native_gate.py analysis/contracts/mission01-final-gate-v3.json --artifact-root . --require JF -> exit 0
python3 tools/audit_ac6_contract_artifacts.py --artifact-root=. analysis/contracts/*.json -> exit 0
python3 tools/audit_ac6_contract_addresses.py --artifact-root=. analysis/contracts/*.json -> exit 0
```

Voir
`recompilation/ace-combat-6-retail/artifacts/retail-us-native-r401-microexec-discriminator/README.md`
pour la méthode complète et les deux specs
(`analysis/microexec/calibration/r401-sub821f9e10-call1-size1.spec`,
`...call2-size256.spec`).

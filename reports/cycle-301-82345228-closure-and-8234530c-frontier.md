# AC6 cycle 301 — fermeture `0x82345228` et front `0x8234530C`

## Question et preuve bornées

Sur `ac6-xbox360-pal`, module `default.xex`, SHA-256
`acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`,
image base `0x82000000`, le contrat headless
`Verify82345228Boundary.java` passe **27/27** assertions.

Il confirme l'unique entrée ABI `0x82345100`, sa frame `0x120`, les trois
compteurs imbriqués `r28/r27/r26`, puis leurs backedges respectifs. À
`0x82345228`, il n'existe ni référence entrante ni entrée Ghidra : l'instruction
transmet `r29` à `r11` entre `subic. r26,r26,1` et le backedge conditionnel
`0x8234522C -> 0x82345134`. Verdict : pseudo-entrée interne **confirmed**.

Les entrées `0x82345250`, `0x82345260` et `0x823452A8` restent inchangées et
non qualifiées.

## Patch, rollback et génération

Seule la ligne `0x82345228 = { name = "rex_sub_82345228" }` est retirée du
TOML. Hash avant :
`6dd8972ebf79a8244f7a606e9e1d8d78c604ee6233511e7a24573212aef646a0` ;
hash après :
`0593b784d34b610177d6146ed53af45d0f18e0939ec021579966cc792fbaa841`.
La réinsertion en flux de cette seule ligne reproduit le hash avant.

ReXGlue termine en **12,59 s**, génère **23 321** fonctions et le runtime lié
a le SHA-256
`63f5ca2d0c164cfb868acc20ff761ed67d09cb371f14f57c9780f24435919132`.
`rex_sub_82345228` disparaît et `rex_sub_82345250` demeure. Aucun généré n'est
modifié manuellement.

## Smoke et validations

Le fatal `0x8234522C -> 0x82345134` est fermé. Le smoke Xvfb/GDB avance à :

```text
Unresolved branch from 0x8234530C to 0x8234524C
```

Cette nouvelle frontière est enregistrée sans classifier les coupures
configurées voisines.

- build runtime `-j16` : PASS ;
- GCC : **44/44 PASS** en 34,48 s ;
- Clang/probes : **48/48 PASS** en 29,33 s ;
- installation racine et garde `bin/bin` : PASS ;
- intervention humaine et GUI Ghidra : aucune.

État : `candidate`, pas `verified`. Prochaine question exacte : déterminer,
par contrat headless distinct, quelle entrée configurée possède le bloc source
`0x8234530C` et la cible interne `0x8234524C`; ne rien retirer par voisinage.

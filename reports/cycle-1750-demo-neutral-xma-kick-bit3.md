# Cycle 1750 — quatrième kick XMA neutral fermé

Neutral headless a été rejoué depuis le store neuf qualifié avec le binaire
codegen-ON après garde explicite des mots observés `1/2/4/8`. Sur la démo PAL
`Default.xex` (`de917873f601e2a2208d75ab907e918ce941a42378d0d088705ecb4477405da8`),
les trois premiers kicks du lot tick 5052 et le quatrième `0x08000000` sont
acceptés; le prochain mot distinct est piégé avant effet.

Preuve directe du quatrième kick désormais traversé :

```text
tick=5052 thread=21
callsite PC=0x82357240, LR=0x823572AC
context=0x2E8000C0
register=0x7FEA1A80
wire=0x08000000, logical_bit=0x00000008
```

Le frontier suivant est le cinquième contexte du même lot :

```text
tick=5052 thread=21
callsite PC=0x82357240, LR=0x823572AC
context=0x2E800100
register=0x7FEA1A80
wire=0x10000000, logical_bit=0x00000010
```

Le trap reste transactionnel : aucun effet XMA, frontend, mission, audio
audible ou pixel non noir n'est promu. Le replay a produit 4915 PRESENT, avec
`frontend=false`, `mission=false`, `terminal=false`; les deux IB restent
`ef7ab6e4…d2b0` (11 dwords) et `d121c8d8…358d6` (3029 dwords).

Artefacts du run frais :

- rapport : `/fastdata/lavaulta/tmp/ac6-cycle1751-proof.yK1Qg9/neutral.report.json`, SHA-256 `287ecfce993fc87031e6de929ec1e73168df7a6aa6c892c0b92cb87c3d55754e` ;
- trace RTPLY-v4 : `/fastdata/lavaulta/tmp/ac6-cycle1751-proof.yK1Qg9/neutral.trace.jsonl`, SHA-256 `6f82300f92f6611c2bcfe67035a740475c27e722567cd33a190b9dadf4ba5ed0` ;
- stderr de qualification : SHA-256 `d47515ebeb5a8139bb22bd8192a61d38f4d05dc135f34b0fd6989a5a563349ae` ;
- binaire codegen-ON : SHA-256 `dab70114197b8ab4a9aa446ede2e1f9ee443086c124fd373933c870b86f6391a` ;
- manifest codegen : SHA-256 `9f1fffb0398358331f9bbf575a3d2fb5cf1478f7cbda5a1dbe46c264a935bbfa`.

Validation ciblée : `ac6-demo-core-tests` et `ac6-demo-complexity` passent (2/2).
La garde suivante doit accepter uniquement `0x20000000` avec le contexte
`0x2E800140`, puis rejouer neutral depuis le même store; START reste gelé.
Le rôle matériel du registre, la consommation des contextes et le décodage
XMA restent `unknown`.

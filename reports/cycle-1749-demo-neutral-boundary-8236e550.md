# Cycle 1749 — frontière neutral `0x8236E550` fermée

Neutral atteint à tick 5049/thread 21 `LR=0x8236E588 -> 0x8236E550` sur la
démo PAL `Default.xex` SHA-256 `de917873…5da8`. La cible est une sous-entrée
du chunk Ghidra canonique `0x8236E530..0x8236E557`, sans entrée `.pdata` :

```text
0x8236E550  3863FFFC  addi r3,r3,-4
0x8236E554  48000144  b    0x8236E698
```

Les huit bytes portent le SHA-256
`76b7caa54b8a23c66e70cad1effbddbf18ead2727a7a739e27a06d6107bfd210`.
La destination `0x8236E698` est une fonction `.pdata` déjà qualifiée. Le rôle
métier et le type propriétaire restent inconnus.

Le codegen strict passe à 12 876 fonctions et 154 records configurés, zéro
diagnostic de frontière et zéro instruction non supportée. Deux atlas frais
sont byte-identiques, SHA-256
`b9728fde73e99d46bcc104fca565885948f15d031edf7f9c390eb8c24df40e3c`,
avec 3 041 220/3 041 220 bytes `.text` classés.

Le replay traverse le thunk et progresse jusqu'au tick 5052/4915
présentations. La prochaine frontière est le quatrième store observé vers
`0x7FEA1A80`, valeur wire `0x08000000`, depuis `LR=0x823572AC`. Il suit un
quatrième `XMACreateContext` et le contexte `0x2E8000C0`; les trois premiers
bits `1/2/4` avaient été qualifiés au tick 1048. L'effet matériel et le
consumer restent inconnus, donc le runtime piège avant effet.

Le rapport porte le SHA-256 `cf1e6578…e47afa`, la trace `6f82300f…a5ed0` et
le binaire `678e2a2e…4a34c`. CTest passe OFF 18/18 et ON 17/17. START,
frontend, pixels non noirs, audio audible et mission restent ouverts.

# AC6 cycle 228 — frontière de transformation du dispatch `0x8211A148`

## Identité et preuve

- Cible : `ac6-xbox360-pal`, module `default.xex`
- SHA-256 :
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Projet Ghidra canonique : `ace-combat-6`
- Fonction : `0x8211A148`, premier mot de dispatch produit par `0x82119740`
  pour les sous-entrées de type `1`.

Le dump Ghidra headless du cycle 227 laissait une lacune de décodage entre
`0x8211A1E4` et `0x8211A278`. La sortie XenonRecomp canonique déjà générée
dans `.tools/recomp-eval/ac6/output/ppc_recomp.10.cpp` qualifie le contrôle et
la frontière d'entrée/sortie de cette région :

1. la branche est prise lorsque le bit 0 des flags est posé, le bit 1 est
   clair et l'octet de contrôle `r6` est non nul ;
2. les quatre mots source à `source+0x14` sont présentés comme un vecteur au
   helper `0x82119620`, qui produit un intermédiaire de 64 octets ;
3. les huit doubles mots de cet intermédiaire sont transmis au helper
   `0x82119458` ;
4. le vecteur résultat `v1` est copié comme quatre mots à la destination et le
   curseur avance de 16 octets.

Cette preuve ne qualifie pas encore les mathématiques VMX128 internes des deux
helpers. La sortie générée n'a pas été modifiée.

## Implémentation native

`copy_function_8211a148_vector` expose maintenant cette composition par un
callback borné `Function8211a148Transform` :

- l'entrée et la sortie sont exactement quatre mots bruts ;
- une transformation acceptée publie les quatre mots et avance le curseur de
  16 octets ;
- un callback absent ou qui refuse l'entrée conserve
  `unresolved_control_path` sans aucune mutation ;
- les validations d'adresses source, destination et curseur précèdent toujours
  la première écriture.

Le callback ne prétend pas être l'implémentation de `0x82119620` ou
`0x82119458`. Il fournit une frontière exacte permettant de raccorder leur
future transcription VMX128 ou un oracle différentiel sans affaiblir le chemin
appelant.

## Validation

La régression ciblée couvre l'absence de callback, une transformation acceptée
et un callback qui refuse l'entrée. Elle vérifie le résultat, l'avance de 16
octets et l'absence de mutation sur refus.

```bash
cmake -S reconstruction/ace-combat-6 -B .build/ace-combat-6 \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build .build/ace-combat-6 -j16 --target ac6-motion-record-tests
ctest --test-dir .build/ace-combat-6 --output-on-failure \
  -R '^ac6-motion-record-tests$'
cmake --build .build/ace-combat-6 -j16
ctest --test-dir .build/ace-combat-6 --output-on-failure -j16
cmake --install .build/ace-combat-6 --prefix "$PWD"
test ! -e bin/bin
git diff --check
```

Résultat ciblé : **1/1 PASS**. Corpus AC6 complet : **42/42 PASS**.
L'installation reste directement sous `bin/`, sans `bin/bin`. Aucun Xenia,
GUI, VNC, asset retail ou geste humain n'a été utilisé.

## Frontière restante

Le contrôle du caller et son contrat de quatre mots sont qualifiés. La
composition mathématique de `0x82119620` puis `0x82119458` reste
`needs-runtime-support` tant que ses instructions VMX128 n'ont pas été
transcrites et comparées. La consommation de ce record par la mission reste
également ouverte ; ce cycle ne constitue pas une preuve de vol, de rendu ou
de parité retail.

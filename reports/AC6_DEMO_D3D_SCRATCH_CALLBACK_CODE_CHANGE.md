# Changement de code — contrat PM4_INTERRUPT

## Ajout

Un modèle C++ autonome représente désormais une interruption du command
processor avec :

```text
source
cpu
cpu_mask original
scratch callback
scratch parameter
```

Le modèle refuse les masques invalides et développe un masque multi-bit en une
requête par CPU.

## Validation locale

```text
c++ -std=c++20 -Wall -Wextra -Wpedantic -Wconversion -Wshadow
xenos CPU interrupt contract: PASS
```

Le vérificateur PAL passe également :

```text
status=PASS
anchors=33
functions=8
producers=5
```

## Limite volontaire

Le type n'est pas encore substitué au `vector<uint8_t>` du runtime. La prochaine
modification doit être faite avec la trace du scratch callback et un A/B sur le
moment de livraison. L'ABI est fermée ; le choix de scheduling ne l'est pas.

# AC6 démo PAL — ABI de retour de `KeSetAffinityThread`

Date : 2026-08-19  
Cible : démo PAL Xbox 360, frontière scheduler/XAudio

## Verdict

Le bridge possédait encore une divergence ABI indépendante de la sélection du
CPU du callback XAudio :

```text
ancienne interprétation : r5 = pointeur de sortie, écrire 1, retourner 0
ABI PAL qualifiée       : retourner le masque précédent dans r3, ne pas toucher r5
```

Le wrapper PAL `0x821A5390` :

1. vérifie un indice processeur dans `0..5` ;
2. construit `1 << indice` ;
3. appelle `KeSetAffinityThread` à `0x821A53DC` ;
4. transforme la valeur de retour par `cntlzw/subfic`.

Cette consommation n'est cohérente qu'avec un masque one-hot retourné dans
`r3`. Il n'existe pas de troisième argument de sortie à ce callsite.

## Conséquence sur les anciennes traces

Les champs auparavant appelés :

```text
previous_ptr
previous_value
previous_mapped
```

étaient dérivés du registre `r5`, qui ne fait pas partie de cette ABI. Les
adresses comme `0x7F040B4C`, `0x7F04097C` ou `0x2EEF0F04` sont donc des valeurs
résiduelles de registre, pas des pointeurs d'affinité qualifiés.

Le bridge pouvait écrire `1` à ces adresses arbitraires. Cette écriture est
retirée. Les anciennes captures restent utiles pour les masques `r4`, les LR,
les objets thread et les ticks, mais pas pour la prétendue valeur précédente.

## Contrat runtime corrigé

Pour un objet thread valide :

```cpp
previous_mask = 1u << current_processor;
requested_processor = bit_index(requested_mask);
thread.processor = requested_processor;
r3 = previous_mask;
```

Contraintes :

```text
requested_mask != 0
requested_mask one-hot
requested_mask limité aux bits 0..5
aucune lecture ou écriture via r5
```

Une forme non qualifiée retourne `false` au dispatcher, qui reste fail-closed.

## Technique d'intégration

Le grand fragment historique est conservé bit à bit sous :

```text
src/guest_bridge/kernel_objects_dispatch_original.hpp
Git blob 957e5f587a9be82f2d7efaf0e44b1a4b840aaf38
```

Le nouveau `kernel_objects_dispatch.hpp` intercepte uniquement
`KeSetAffinityThread`, puis inclut le fragment original pour tous les autres
imports. Cela évite une réécriture bruyante d'un gros fichier et rend la
correction facile à retirer ou auditer.

## Effet sur XAudio

Cette correction ne choisit toujours pas CPU 4 ou CPU 5 pour le callback
synthétique. Elle retire cependant deux perturbations susceptibles de polluer
l'A/B :

- corruption de mémoire guest par écriture via un faux pointeur `r5` ;
- retour constant `0` au lieu du masque précédent.

Les affinités audio observées `0x10`, `0x10`, `0x20` restent donc des preuves de
CPU 4, 4 et 5, tandis que les anciennes valeurs `previous_ptr/value` sont
rétractées.

## Validation

Le test autonome couvre les six masques one-hot, les formes invalides et les
transitions CPU 0→4 et CPU 4→5. Le vérificateur de source impose :

- conservation exacte du fragment original ;
- interception avant délégation ;
- retour du masque précédent dans `r3` ;
- absence d'écriture via `r5` ;
- suppression du workflow Actions temporaire.

## Front suivant

```text
CPU 4/5 explicite autour du callback
→ handle exact signalé dans 0x82355E58
→ waiter réveillé
→ frame 0x1800 soumise
→ progression XMA
```

Le choix CPU ne doit être promu qu'après cette chaîne, pas après la seule
disparition d'un trap.

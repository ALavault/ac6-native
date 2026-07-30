# Cycle 329 — l'invité tourne à 100 % sur un `ud2`, et le runtime le cachait

## Cible

- Produit : AC6 Xbox 360 PAL, Xenon PPC big-endian, Xenos
- Module : `default.xex`, SHA-256
  `acc302c1599c7a2fd38bd5a7de395b418a157d7001b6f986ab7113f45711bcde`
- Binaire mesuré : `5fbe1dfbf56ee0ef354588957cf87ca4f7467af20e0d57c7b9b37c8e66bc77b4`
- Correctif du cycle 328 actif (interposeur), sans quoi le thread dort

## 0. Objet

P0.2 ter. Le cycle 328 a fait passer le thread principal de **0,0 % de CPU,
garé dans une réception réseau** à **121 % de CPU, actif**. La question change de
nature : non plus « sur quoi attend-il ? » mais « quelle boucle exécute-t-il ? ».

C'est le premier cycle où cette mesure a des échantillons à prendre. Le cycle 326
avait montré qu'un profil du thread principal était vide tant qu'il dormait.

## 1. Profil, thread principal seul

`tools/ac6-main-thread-profile.sh`. Restreint à un seul thread par `perf -t` :
un enregistrement DWARF de tout le processus sur ce binaire LTO de 165 Mo produit
des gigaoctets et capture surtout le worker audio, dont le cycle 326 a déjà
montré qu'il brûle un cœur sans être l'invité.

Profil plat, 601 échantillons sur la fonction chaude :

| part | symbole |
|---:|---|
| ~40 % cumulés | adresses noyau (`[k] 0xffffffffb1001…`) |
| 6,04 % | `__imp__sub_8237C828` |
| 2,04 % | `rex::arch::ExceptionHandlerCallback` |
| 0,21 % | `rex::runtime::MMIOHandler::ExceptionCallback` |

Le rapport **2,04 % contre 0,21 %** est le premier indice décisif : dix fois plus
d'exceptions sont traitées que d'accès MMIO. La majorité des fautes **ne sont pas
de la MMIO**, alors que la MMIO est la seule raison légitime pour laquelle ce
runtime prend des fautes.

Chaîne d'appels invitée reconstituée :

```
xstart -> … -> sub_821D7D90 -> sub_821D7A90 -> sub_822AAFC8
      -> sub_821B04D0 -> sub_820D8FE0 -> sub_8237B4D8
      -> sub_8237CC58 -> sub_8237C828        <- feuille chaude
```

## 2. La preuve : `perf annotate`

```text
Percent | Disassembly of __imp__sub_8237C828 (601 samples)
   0.00 : movbel (%rsi,%r9), %r9d      ; charge l'entrée de table de saut
   0.00 : cmpl   $0x2, %r9d
   0.00 : jb     sub_8237C828+0x10     ; les deux « cas »
   0.00 : …
 100.00 : ud2                          <- 100 % des échantillons
```

**100,00 % des échantillons de cette fonction tombent sur une seule instruction :
`ud2`.** Il n'y a pas de boucle chaude invitée à chercher : le thread ne fait
rien d'autre que déclencher, encore et encore, la même instruction illégale.

## 3. Défaut 1 — un `bctr` de table de saut mal traduit

Le source généré :

```c
	// lis r11,-32152 / addi r11,r11,-24112     -> r11 = 0x8267A1D0, base de table
	// lwzx r11,r10,r11                          -> r11 = table[index], une ADRESSE
	ctx.r11.u64 = PPC_LOAD_U32(ctx.r10.u32 + ctx.r11.u32);
	// mtctr r11
	ctx.ctr.u64 = ctx.r11.u64;
	// bctr
	switch (ctx.r11.u32) {
	case 0: goto loc_8237C828;
	case 1: goto loc_8237C828;
	default: __builtin_trap(); // Switch case out of range
	}
```

Le générateur a transformé un saut calculé en un `switch` **sur l'adresse
chargée**, avec deux cas ordinaux. Or la valeur chargée est une adresse de code
invité, donc `>= 0x82000000` : la comparaison `cmpl $0x2` **ne peut jamais**
être vraie. Les deux cas sont du code mort et le `default` est le seul chemin
atteignable. C'est ce que l'assembleur montre, et ce que le profil confirme.

Étendue mesurée sur le corpus : **751 aiguillages** de ce genre, dont **742
légitimes** (plusieurs cibles distinctes, indices réels) et **9 dégénérés** —
ordinaux minuscules, une seule cible :

```
sub_821277A0   cases=4  -> loc_82127868      sub_8237BF08   cases=1  -> loc_8237BF08
sub_82162D50   cases=7  -> loc_82162E28      sub_8237C828   cases=2  -> loc_8237C828  <- ici
sub_82318694   cases=3  -> loc_8231874C      rex_sub_823192C0 cases=1 -> loc_82319348
sub_82318694   cases=1  -> loc_82319288      sub_8226C388   cases=9  -> loc_8226CDB0
```

Le défaut est donc **étroit et énumérable**, pas systémique : 9 sites sur 751.

## 4. Défaut 2 — le runtime transformait un arrêt dur en boucle silencieuse

`exception_handler_posix.cpp` installe un gestionnaire pour `SIGILL` **et**
`SIGSEGV`. Il parcourt les gestionnaires enregistrés ; si aucun ne réclame la
faute, la fonction **retourne simplement** :

```c
  for (…) { if (handlers_[i].first(&ex, …)) { …; return; } }
}   // <- aucun gestionnaire n'a réclamé : on retourne quand même
```

Retourner d'un gestionnaire de signal **sans avoir avancé le compteur ordinal**
reprend l'exécution **sur la même instruction**. Le `ud2` refaute, immédiatement,
indéfiniment. D'où le tableau observé :

- ~40 % du temps dans le noyau : c'est la machinerie de délivrance de signal ;
- `ExceptionHandlerCallback` chaud, `MMIOHandler::ExceptionCallback` froid ;
- 121 % de CPU, aucune trame, **aucun plantage, aucun message**.

Le processus paraissait vivant. C'est le défaut le plus coûteux des deux : il
n'est pas la cause du blocage, il en est le **camouflage**. Localiser une
instruction illégale a demandé un profil et une passe d'annotation ; cela aurait
dû coûter une ligne de journal.

C'est exactement la classe de défaut traitée au cycle 312 pour les cibles
indirectes non enregistrées, et la même leçon : *un défaut silencieux et
diagnosticable en plusieurs cycles doit devenir une ligne de journal.*

## 5. Correctif appliqué (défaut 2)

Le repli du gestionnaire restaure la disposition par défaut, journalise le
compteur ordinal fautif et l'adresse, puis laisse l'instruction se réexécuter —
cette fois prise par la disposition par défaut, qui termine le processus.
Échouer bruyamment vaut strictement mieux que boucler en silence.

Patch : `patches/rexglue-fail-loud-on-unhandled-fault-20260730.patch`
(compilé, `rc=0`, `git apply --check` vert sur l'arbre de référence).

**Le défaut 1 n'est pas corrigé.** C'est lui qui bloque l'invité, et il vit dans
la résolution des tables de saut du générateur — donc dans une régénération du
corpus, pas dans un correctif d'exécution. C'est le travail du cycle suivant.

## 6. Validation exécutée

```text
profil du thread principal, -t tid, 20 s        601 échantillons exploitables
perf annotate sur la fonction chaude            100,00 % sur une seule `ud2`
recensement des aiguillages du corpus           751 total, 9 dégénérés, énumérés
lecture du repli du gestionnaire d'exceptions   retour sans avance du PC, confirmé
compilation exception_handler_posix.cpp         rc=0, 0 erreur
git apply --check sur l'arbre de référence      OK
```

Ni preuve de jouabilité, ni preuve de parité retail.
`recompiler-generated` n'est pas `verified`.

## 7. Porte P0

**Non franchie**, inchangée depuis le cycle 328 : `eop` 34,
`host_swap_presents` 12, `wptr` figé. Ce cycle n'améliore aucun compteur — il
**nomme la cause** du gel actuel et supprime le camouflage qui la cachait.

## 8. Front suivant

`sub_8237C828` est atteint, exécute son `ud2`, et n'en sort pas. Pour que
l'invité avance, il faut que ce `bctr` saute réellement.

1. Lire la table de saut à `0x8267A1D0` dans l'image et énumérer ses cibles
   réelles — la même méthode qu'aux cycles 307 et 312 pour les adresses absentes
   de `[functions]`.
2. Déterminer pourquoi le générateur a produit des cas ordinaux là où il fallait
   une répartition par adresse ; les 742 aiguillages corrects montrent que le
   mécanisme fonctionne en général, donc la question est ce qui distingue ces 9.
3. Traiter les 8 autres sites au même passage : ils sont énumérés, et chacun est
   une boucle infinie latente sur le chemin où il se trouve.

## 9. Règles ajoutées

1. **Un profil dont la fonction chaude est à 100 % sur une instruction n'est pas
   une boucle chaude, c'est une faute répétée.** Le réflexe « chercher la boucle
   invitée » aurait fait lire le code invité autour ; `perf annotate` a donné la
   réponse en une commande.
2. **Comparer les compteurs d'exceptions entre eux.** `ExceptionHandlerCallback`
   dix fois plus chaud que `MMIOHandler::ExceptionCallback` disait déjà que la
   majorité des fautes n'étaient pas de la MMIO, seule raison légitime d'en
   prendre ici. Ce rapport était le premier signal, avant toute annotation.
3. **Un gestionnaire de signal qui retourne sans avancer le PC ni réémettre
   fabrique une boucle infinie silencieuse.** À vérifier dans tout runtime qui
   installe `SIGSEGV`/`SIGILL` pour un usage légitime : le chemin de repli est
   aussi important que le chemin nominal.

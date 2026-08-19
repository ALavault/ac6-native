# Vérification finale

- [x] Le contrat mappe exactement les offsets `0x0C..0x38` des six paires.
- [x] CPU 0 est refusé lorsque sa paire est nulle.
- [x] CPU 4 et CPU 5 sont acceptés uniquement sur demande explicite.
- [x] L'état à deux paires complètes est refusé en sélection implicite.
- [x] Une paire unique peut être inférée sans heuristique.
- [x] Un processeur hors `0..5` et une paire incomplète sont refusés.
- [x] Le test C++20 passe avec warnings stricts.
- [x] Aucun comportement runtime par défaut n'est modifié.
- [x] Aucun ZIP ni octet propriétaire n'est publié.

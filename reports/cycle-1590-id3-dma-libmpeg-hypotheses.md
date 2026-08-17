# Cycle 1590 — `id3`: DMA et libmpeg restent des hypothèses

## Verdict

`id3` n'est pas un symbole qualifié dans le dépôt actuel. La recherche ciblée
ne trouve ni identifiant autonome `id3`, ni bibliothèque ou symbole
`libmpeg`, ni référence DMA dans le code natif AC6. Il est donc interdit de
classer cette entrée comme codec, transfert DMA ou fonction gameplay sur son
seul numéro.

## Pistes conservées

* **DMA** : vérifier la chaîne d'appel, les adresses source/destination, les
  tailles alignées, les anneaux/descripteurs et les écritures d'interruption.
  Un contrôle positif doit montrer une copie observable et bornée avec le
  même contrat d'achèvement.
* **MPEG** : vérifier les consommateurs de `RadioTbl`/`TextData`, les banques
  `RIFF/XMA` ou `ASF`, les en-têtes de paquets, le rythme d'échantillons et la
  sortie PCM. Une simple dépendance FFmpeg ne constitue pas une preuve de
  `libmpeg` retail.

## Évidence locale

Le chemin média natif contient actuellement `retail_media.cpp` et
`retail_asf_index.cpp` : il reconnaît les banques `RIFF/XMA` et `ASF`, et
utilise l'API FFmpeg générique pour le décodage borné. Les lecteurs
`RadioTblBin` sont dans `retail_bin_readers.cpp`. Aucun de ces éléments ne
relie encore un identifiant `id3` à une fonction retail qualifiée.

Le census d’imports du `default.xex` PAL ajoute une borne utile, sans fermer
la question : le binaire importe `XMACreateContext`/`XMAReleaseContext` et
les appels `XAudioRegisterRenderDriverClient`,
`XAudioSubmitRenderDriverFrame` et les accès de volume XAudio. Le même census
ne contient ni symbole `libmpeg`, ni export nommé DMA. Cela qualifie une
frontière XMA/XAudio Xenon réelle pour la chaîne média, mais un transfert DMA
interne ou un décodeur MPEG encapsulé reste possible et non identifié ; les
deux hypothèses restent donc provisoires.

## Condition de promotion

Ne promouvoir `id3` qu'après qualification du projet Ghidra canonique, du
XEX PAL et de la plage exacte, puis un contrôle positif et un rejet borné pour
chaque hypothèse concurrente. En attendant, l'entrée reste
`unclassified/provisional` et ne ferme pas la lane XMA/ASF.

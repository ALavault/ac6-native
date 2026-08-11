# Cycle 1521 — reprise retail avec séquenceur complet

## Résultat

Les checkpoints `RetailSession` conservent désormais, en plus du curseur du
script et de `MissionExecution`, l'état complet de `SubMissionSequencer` :
forme du scénario, sous-mission sélectionnée, étape, horodatages de départ,
valeurs de compteurs, horodatages de premier passage à un et bits associés.

Le bloc est sérialisé dans une enveloppe locale `RSQ1`, bornée à 4 096
entrées et 1 MiB. Le format de sauvegarde atomique passe en v12 ; les versions
v1 à v11 restent lisibles, mais une reprise retail exige le bloc v12. Les
flottants non finis, formes incompatibles, blobs tronqués ou excédentaires et
désaccords entre sous-mission du script et du séquenceur sont refusés. Une
restauration échouée remet le script et le séquenceur dans leur état précédent.

Aucun octet retail, code généré, RexGlue ni observation AC6_recomp n'entre dans
cet incrément. Il ferme la frontière explicitement laissée ouverte au cycle
1520 ; les producteurs IA/combat futurs devront alimenter les mêmes compteurs,
mais leur état déjà publié survivra maintenant à une sauvegarde/reprise.

## Gardes de non-régression

- Round-trip du format v12 avec le bloc séquenceur et rejet du bloc présent
  sans marqueur de script.
- Snapshot/restauration exacte du curseur, des compteurs et horodatages ; rejet
  non mutatif d'une forme ou d'un flottant invalide.
- Rejet d'un bloc `RSQ1` bien formé qui nomme une autre sous-mission que le
  curseur retail.
- Poursuite d'une session au frame hash identique après checkpoint.

## Validation

- Build produit `ac6-native` : passe.
- CTest complet : 70/70 passent ; les deux tests dépendant d'une ressource
  externe (`ac6-retail-frontend-resources`, `ac6-vulkan-surface-smoke`) sont
  explicitement sautés par leur contrat.
- Tests Python : 88/88 passent.
- Audits Mission 01 : JF, J0/J1 et class-map J2 passent après rafraîchissement
  mécanique des hashes des deux dérivations modifiées.
- Audit des artefacts cités contre le commit : passe après commit.

## Risque résiduel

Le format v12 n'apporte pas encore de migration permettant de reprendre une
session retail depuis une sauvegarde v11 : celle-ci reste lisible pour
inspection, mais est refusée au chargement faute d'état de compteurs qualifié.

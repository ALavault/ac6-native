# Cycle 1806 — témoin codegen ancien : ring positif, readback noir

Le binaire codegen-on conservé du 22 août a été exécuté avec la démo PAL, le
store, Vulkan et la borne de 1160 ticks déjà qualifiés. Il produit 502
soumissions ring, 4405 dwords et 407 présentations renderer, contre zéro pour
le binaire courant.

L'attribution statique des symboles et le désassemblage montrent que les deux
binaires possèdent le test de l'adresse `0x7FC80714` dans
`AC6_PPC_STORE_U32` et l'appel vers `GuestBridge::apply_xenos_mmio_write`.
La divergence est donc antérieure au hook, dans l'exécution guest ou son
ordonnancement natif.

La capture obligatoire a été produite et inspectée. Elle est uniformément
noire; les soumissions PM4 seules ne constituent donc pas une validation
visuelle du frontend.

Preuve complète :
`artifacts/goal-playable/pm4-old-codegen-on-b-1160-20260824/RESULT.md`.

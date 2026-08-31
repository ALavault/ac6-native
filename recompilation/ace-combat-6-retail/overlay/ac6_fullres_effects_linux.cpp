#include "ac6_fullres_effects.h"

#include <rex/cvar.h>

REXCVAR_DEFINE_BOOL(ac6_fullres_effects, false, "AC6/Enhancements",
                    "Unavailable in the stock Linux/Vulkan retail gate");

namespace ac6::backend {

void NoteRt0Bound(uint32_t, uint32_t) {}
uint32_t LastRt0Bound() { return 0; }
void FullresFixResolveRect(PPCContext&, uint8_t*) {}
void FxNoteResolveDest(uint32_t, uint8_t*) {}
void FxCropCompositorMask(uint64_t, uint64_t, uint32_t*) {}
bool FxFixDownscalerDraw(uint64_t, uint32_t*, uint32_t*, uint32_t*, uint32_t*,
                         uint32_t*) {
  return false;
}

}  // namespace ac6::backend

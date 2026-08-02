#pragma once

#include <rex/cvar.h>
#include <rex/ppc/types.h>

#include "d3d_state.h"

REXCVAR_DECLARE(bool, ac6_render_capture);

// PAL retail MATE draw boundary at 0x82364B44. r30 is the request and r31 is
// the D3D device. The tuple is attached once to the next compatible captured
// guest draw; it is diagnostic evidence, not a host-render success marker.
void ac6MateDrawRequestHook(PPCRegister& r30, PPCRegister& r31);

namespace ac6::d3d {

void OnFrameBoundary();

DrawStatsSnapshot GetDrawStats();
FrameCaptureSnapshot TakeFrameCapture(FrameCaptureSummary* summary_out = nullptr);
ShadowState GetShadowState();

}  // namespace ac6::d3d

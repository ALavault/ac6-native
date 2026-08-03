// Headless definitions required by the XEX loader used by the code generator.
// This file is compiled only when REXGLUE_CODEGEN_ONLY is enabled.

#include <rex/cvar.h>
#include <rex/ui/windowed_app_context.h>

REXCVAR_DEFINE_INT32(window_width, 0, "UI/Window",
                     "Startup window width in logical pixels (codegen only)");
REXCVAR_DEFINE_INT32(window_height, 0, "UI/Window",
                     "Startup window height in logical pixels (codegen only)");
REXCVAR_DEFINE_INT32(video_mode_width, 1280, "GPU",
                     "Guest video mode width in pixels (codegen only)");
REXCVAR_DEFINE_INT32(video_mode_height, 720, "GPU",
                     "Guest video mode height in pixels (codegen only)");
REXCVAR_DEFINE_STRING(resolution, "", "GPU", "Resolution preset (codegen only)");
REXCVAR_DEFINE_DOUBLE(video_mode_refresh_rate, 60.0, "GPU",
                      "Guest video mode refresh rate in Hz (codegen only)");

namespace rex::ui {

bool WindowedAppContext::CallInUIThreadSynchronous(std::function<void()> /*function*/) {
  // A tool-only process has no UI thread. Returning false makes callers use
  // their existing headless/error path rather than executing window code.
  return false;
}

}  // namespace rex::ui

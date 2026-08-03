#pragma once

#include <memory>
#include <string>

#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/rex_app.h>
#if REX_HAS_D3D12
#include <rex/graphics/d3d12/graphics_system.h>
#endif
#if REX_HAS_VULKAN
#include <rex/graphics/vulkan/graphics_system.h>
#endif

#include "ac6_native_graphics.h"
#include "ac6_native_graphics_overlay.h"
#include "d3d_hooks.h"
#include "render_hooks.h"
#include "generated/ac6recomp_config.h"
#include <rex/ppc/context.h>

PPC_EXTERN_FUNC(sub_820F6180);

REXCVAR_DECLARE(std::string, ac6_graphics_backend);
REXCVAR_DECLARE(bool, ac6_show_diagnostics_overlay);

class Ac6recompApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  Ac6recompApp(rex::ui::WindowedAppContext& ctx, std::string_view name, rex::PPCImageInfo ppc_info)
      : rex::ReXApp(ctx, name, ppc_info) {
    REXLOG_INFO("Ac6recompApp constructor");
  }

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    REXLOG_INFO("Ac6recompApp::Create");
    return std::unique_ptr<Ac6recompApp>(new Ac6recompApp(ctx, "ac6recomp", PPCImageConfig));
  }

 protected:
  void OnPreSetup(rex::RuntimeConfig& config) override {
    REXLOG_INFO("Ac6recompApp::OnPreSetup");
    rex::ReXApp::OnPreSetup(config);

    const std::string requested_backend = REXCVAR_GET(ac6_graphics_backend);
#if REX_HAS_VULKAN
    if (requested_backend == "vulkan" || requested_backend == "auto") {
      config.graphics =
          REX_GRAPHICS_BACKEND(rex::graphics::vulkan::VulkanGraphicsSystem);
      REXLOG_INFO("Ac6recompApp: selected Vulkan graphics backend");
      return;
    }
#endif
#if REX_HAS_D3D12
    if (requested_backend == "d3d12" || requested_backend == "auto") {
      config.graphics =
          REX_GRAPHICS_BACKEND(rex::graphics::d3d12::D3D12GraphicsSystem);
      REXLOG_INFO("Ac6recompApp: selected D3D12 graphics backend");
      return;
    }
#endif
    REXLOG_WARN("Ac6recompApp: requested graphics backend '{}' is not available in this build",
                requested_backend);
  }

  void OnPostSetup() override {
    REXLOG_INFO("Ac6recompApp::OnPostSetup");
    rex::ReXApp::OnPostSetup();

    // 0x820F6180 is a valid PAL vtable thunk, but XenonRecomp did not emit a
    // function-map entry for this interior thunk. Register the native bridge
    // after Runtime::Setup() has allocated the guest dispatch table and before
    // the module thread starts executing the XEX.
    if (runtime() && runtime()->function_dispatcher()) {
      const bool registered =
          runtime()->function_dispatcher()->SetFunction(0x820F6180, sub_820F6180);
      REXLOG_INFO("Ac6recompApp: registered PAL thunk 0x820F6180 ({})",
                  registered ? "ok" : "failed");
    }

    auto* graphics_sys = runtime()->graphics_system();
    if (graphics_sys) {
        graphics_sys->SetFrameBoundaryCallback([](rex::memory::Memory* memory) {
            ::ac6::graphics::OnFrameBoundary(memory);
        });
        REXLOG_INFO("Ac6recompApp: Native frame boundary callback registered");
    }
  }

  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    REXLOG_INFO("Ac6recompApp::OnCreateDialogs");
    native_graphics_status_dialog_ =
        std::make_unique<ac6::graphics::NativeGraphicsStatusDialog>(drawer);
    // Off by default. This panel covers the left 40% of a 1280x720 frame, which
    // is most of a dialog and all of the left-hand button on a two-option one --
    // so with it up, the thing under investigation is partly hidden, and every
    // screenshot in this investigation has been taken through it. It is also a
    // diagnostic displayed unconditionally, which a shipping build should not do.
    if (REXCVAR_GET(ac6_show_diagnostics_overlay)) {
      native_graphics_status_dialog_->Show();
    }

    // Feed the F3 debug overlay the game's own frame stats so its guest
    // frametime graph and counter reflect the real sim cadence (SDK cannot
    // reach into the AC6 app layer, so it takes them through this callback).
    // Must be here, not OnPostSetup: the debug overlay is created just before
    // OnCreateDialogs, but OnPostSetup runs BEFORE the overlay exists, so a
    // provider set there is silently dropped.
    SetGuestFrameStats([]() -> rex::ui::FrameStats {
        const ::ac6::FrameStats s = ::ac6::GetFrameStats();
        return rex::ui::FrameStats{s.frame_time_ms, s.fps, s.frame_count};
    });
  }

 private:
  std::unique_ptr<ac6::graphics::NativeGraphicsStatusDialog> native_graphics_status_dialog_;
};

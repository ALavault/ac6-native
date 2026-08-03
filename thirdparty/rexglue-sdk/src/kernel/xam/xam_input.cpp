/**
 * ReXGlue runtime - AC6 Recompilation project
 * Copyright (c) 2026 Tom Clay. All rights reserved.
 */

#include <atomic>
#include <map>
#include <mutex>
#include <rex/cvar.h>
#include <rex/system/thread_state.h>
#include <rex/input/input.h>
#include <rex/input/input_system.h>
#include <rex/kernel/xam/private.h>
#include <rex/logging.h>
#include <rex/ppc/function.h>
#include <rex/ppc/types.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xtypes.h>

#pragma GCC diagnostic ignored "-Wunused-parameter"

namespace rex {
namespace kernel {
namespace xam {

REXCVAR_DEFINE_BOOL(ac6_log_input_callers, false, "AC6/Input",
                   "Log which guest routine polls XamInputGetState, by link-register address.");

using namespace rex::system;

using rex::input::X_INPUT_CAPABILITIES;
using rex::input::X_INPUT_KEYSTROKE;
using rex::input::X_INPUT_STATE;
using rex::input::X_INPUT_VIBRATION;

constexpr uint32_t XINPUT_FLAG_GAMEPAD = 0x01;
constexpr uint32_t XINPUT_FLAG_ANY_USER = 1 << 30;

rex::input::InputSystem* input_system() {
  return REX_KERNEL_STATE()->input_system();
}

void XamResetInactivity_entry() {
  // Do we need to do anything?
}

ppc_u32_result_t XamEnableInactivityProcessing_entry(ppc_u32_t unk, ppc_u32_t enable) {
  return X_ERROR_SUCCESS;
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/microsoft.directx_sdk.reference.xinputgetcapabilities(v=vs.85).aspx
ppc_u32_result_t XamInputGetCapabilities_entry(ppc_u32_t user_index, ppc_u32_t flags,
                                               ppc_ptr_t<X_INPUT_CAPABILITIES> caps) {
  REXKRNL_TRACE("[XAM] XamInputGetCapabilities called: user={}, flags=0x{:X}", (uint32_t)user_index,
                (uint32_t)flags);
  if (!caps) {
    return X_ERROR_BAD_ARGUMENTS;
  }

  if ((flags & 0xFF) && (flags & XINPUT_FLAG_GAMEPAD) == 0) {
    // Ignore any query for other types of devices.
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  uint32_t actual_user_index = user_index;
  if ((actual_user_index & 0xFF) == 0xFF || (flags & XINPUT_FLAG_ANY_USER)) {
    // Always pin user to 0.
    actual_user_index = 0;
  }

  auto* is = input_system();
  return is->GetCapabilities(actual_user_index, flags, caps);
}

ppc_u32_result_t XamInputGetCapabilitiesEx_entry(ppc_u32_t unk, ppc_u32_t user_index,
                                                 ppc_u32_t flags,
                                                 ppc_ptr_t<X_INPUT_CAPABILITIES> caps) {
  if (!caps) {
    return X_ERROR_BAD_ARGUMENTS;
  }

  if ((flags & 0xFF) && (flags & XINPUT_FLAG_GAMEPAD) == 0) {
    // Ignore any query for other types of devices.
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  uint32_t actual_user_index = user_index;
  if ((actual_user_index & 0xFF) == 0xFF || (flags & XINPUT_FLAG_ANY_USER)) {
    // Always pin user to 0.
    actual_user_index = 0;
  }

  (void)unk;  // Unused in this implementation
  auto* is = input_system();
  return is->GetCapabilities(actual_user_index, flags, caps);
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/microsoft.directx_sdk.reference.xinputgetstate(v=vs.85).aspx
ppc_u32_result_t XamInputGetState_entry(ppc_u32_t user_index, ppc_u32_t flags,
                                        ppc_ptr_t<X_INPUT_STATE> input_state) {
  // Games call this with a NULL state ptr, probably as a query.
  static int call_count = 0;
  if (++call_count <= 5) {
    REXKRNL_TRACE("[XAM] XamInputGetState called: user={}, flags=0x{:X}", (uint32_t)user_index,
                  (uint32_t)flags);
  }

  if ((flags & 0xFF) && (flags & XINPUT_FLAG_GAMEPAD) == 0) {
    // Ignore any query for other types of devices.
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  uint32_t actual_user_index = user_index;
  if ((actual_user_index & 0xFF) == 0xFF || (flags & XINPUT_FLAG_ANY_USER)) {
    // Always pin user to 0.
    actual_user_index = 0;
  }

  auto* is = input_system();
  const uint32_t result = is->GetState(actual_user_index, input_state);

  // P1.3: which GUEST function is polling the pad.
  //
  // Cycles 397-407 exhausted the host side: presentation, the render loop,
  // input delivery, thread scheduling and the viewport transform are all
  // measured correct, yet the save screen redraws an unchanging image and acts
  // on none of the buttons it provably receives. The remaining question is
  // guest-side -- what state the UI believes it is in -- and the way in is the
  // caller of this function, since that is the entry to whatever state machine
  // consumes the pad.
  //
  // The link register still holds the return address into recompiled guest
  // code, so it names the calling guest routine. Counted rather than printed
  // per call: this runs at frame rate.
  if (REXCVAR_GET(ac6_log_input_callers)) {
    static std::mutex callers_mutex;
    static std::map<uint64_t, uint64_t> callers;
    static uint64_t polls = 0;
    if (auto* ts = rex::runtime::ThreadState::Get(); ts) {
      const uint64_t lr = rex::runtime::current_ppc_context()->lr;
      std::lock_guard<std::mutex> lock(callers_mutex);
      ++callers[lr];
      if ((++polls % 600) == 0) {
        for (const auto& [addr, count] : callers) {
          REXKRNL_INFO("[ac6-input-caller] lr=0x{:08X} polls={}", addr, count);
        }
      }
    }
  }

  // P1.2: what the GUEST actually receives.
  //
  // The MnK probe proved the driver holds a pressed key, but that is one layer
  // short of the claim. This is the boundary that matters: the button mask
  // written into the guest's X_INPUT_STATE, plus the result code, because a
  // DEVICE_NOT_CONNECTED here means the guest is told there is no controller
  // no matter what the driver knows.
  //
  // Logged only on transition, so a held button produces two lines rather than
  // one per poll at frame rate.
  {
    static std::atomic<uint32_t> last_buttons{0};
    static std::atomic<uint32_t> last_result{0xFFFFFFFFu};
    const uint32_t buttons =
        input_state ? static_cast<uint32_t>(input_state->gamepad.buttons) : 0u;
    if (buttons != last_buttons.exchange(buttons, std::memory_order_relaxed) ||
        result != last_result.exchange(result, std::memory_order_relaxed)) {
      REXKRNL_INFO("[xam-input] guest sees buttons=0x{:04X} result=0x{:X} user={} state_ptr={}",
                   buttons, result, actual_user_index, input_state ? "yes" : "null");
    }
  }
  return result;
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/microsoft.directx_sdk.reference.xinputsetstate(v=vs.85).aspx
ppc_u32_result_t XamInputSetState_entry(ppc_u32_t user_index, ppc_u32_t unk,
                                        ppc_ptr_t<X_INPUT_VIBRATION> vibration) {
  if (!vibration) {
    return X_ERROR_BAD_ARGUMENTS;
  }

  uint32_t actual_user_index = user_index;
  if ((user_index & 0xFF) == 0xFF) {
    // Always pin user to 0.
    actual_user_index = 0;
  }

  (void)unk;  // Unused in this implementation
  auto* is = input_system();
  return is->SetState(actual_user_index, vibration);
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/microsoft.directx_sdk.reference.xinputgetkeystroke(v=vs.85).aspx
ppc_u32_result_t XamInputGetKeystroke_entry(ppc_u32_t user_index, ppc_u32_t flags,
                                            ppc_ptr_t<X_INPUT_KEYSTROKE> keystroke) {
  // https://github.com/CodeAsm/ffplay360/blob/master/Common/AtgXime.cpp
  // user index = index or XUSER_INDEX_ANY
  // flags = XINPUT_FLAG_GAMEPAD (| _ANYUSER | _ANYDEVICE)

  if (!keystroke) {
    return X_ERROR_BAD_ARGUMENTS;
  }

  if ((flags & 0xFF) && (flags & XINPUT_FLAG_GAMEPAD) == 0) {
    // Ignore any query for other types of devices.
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  uint32_t actual_user_index = user_index;
  if ((actual_user_index & 0xFF) == 0xFF || (flags & XINPUT_FLAG_ANY_USER)) {
    // Always pin user to 0.
    actual_user_index = 0;
  }

  auto* is = input_system();
  return is->GetKeystroke(actual_user_index, flags, keystroke);
}

// Same as non-ex, just takes a pointer to user index.
ppc_u32_result_t XamInputGetKeystrokeEx_entry(ppc_pu32_t user_index_ptr, ppc_u32_t flags,
                                              ppc_ptr_t<X_INPUT_KEYSTROKE> keystroke) {
  if (!keystroke) {
    return X_ERROR_BAD_ARGUMENTS;
  }

  if ((flags & 0xFF) && (flags & XINPUT_FLAG_GAMEPAD) == 0) {
    // Ignore any query for other types of devices.
    return X_ERROR_DEVICE_NOT_CONNECTED;
  }

  uint32_t user_index = *user_index_ptr;
  if ((user_index & 0xFF) == 0xFF || (flags & XINPUT_FLAG_ANY_USER)) {
    // Always pin user to 0.
    user_index = 0;
  }

  auto* is = input_system();
  auto result = is->GetKeystroke(user_index, flags, keystroke);
  if (XSUCCEEDED(result)) {
    *user_index_ptr = keystroke->user_index;
  }
  return result;
}

ppc_hresult_result_t XamUserGetDeviceContext_entry(ppc_u32_t user_index, ppc_u32_t unk,
                                                   ppc_pu32_t out_ptr) {
  // Games check the result - usually with some masking.
  // If this function fails they assume zero, so let's fail AND
  // set zero just to be safe.
  *out_ptr = 0;
  if (!user_index || (user_index & 0xFF) == 0xFF) {
    return X_E_SUCCESS;
  } else {
    return X_E_DEVICE_NOT_CONNECTED;
  }
}

}  // namespace xam
}  // namespace kernel
}  // namespace rex

XAM_EXPORT(__imp__XamResetInactivity, rex::kernel::xam::XamResetInactivity_entry)
XAM_EXPORT(__imp__XamEnableInactivityProcessing,
           rex::kernel::xam::XamEnableInactivityProcessing_entry)
XAM_EXPORT(__imp__XamInputGetCapabilities, rex::kernel::xam::XamInputGetCapabilities_entry)
XAM_EXPORT(__imp__XamInputGetCapabilitiesEx, rex::kernel::xam::XamInputGetCapabilitiesEx_entry)
XAM_EXPORT(__imp__XamInputGetState, rex::kernel::xam::XamInputGetState_entry)
XAM_EXPORT(__imp__XamInputSetState, rex::kernel::xam::XamInputSetState_entry)
XAM_EXPORT(__imp__XamInputGetKeystroke, rex::kernel::xam::XamInputGetKeystroke_entry)
XAM_EXPORT(__imp__XamInputGetKeystrokeEx, rex::kernel::xam::XamInputGetKeystrokeEx_entry)
XAM_EXPORT(__imp__XamUserGetDeviceContext, rex::kernel::xam::XamUserGetDeviceContext_entry)

XAM_EXPORT_STUB(__imp__XamInputControl);
XAM_EXPORT_STUB(__imp__XamInputEnableAutobind);
XAM_EXPORT_STUB(__imp__XamInputGetDeviceStats);
XAM_EXPORT_STUB(__imp__XamInputGetFailedConnectionOrBind);
XAM_EXPORT_STUB(__imp__XamInputGetKeyLocks);
XAM_EXPORT_STUB(__imp__XamInputGetKeystrokeHud);
XAM_EXPORT_STUB(__imp__XamInputGetKeystrokeHudEx);
XAM_EXPORT_STUB(__imp__XamInputGetUserVibrationLevel);
XAM_EXPORT_STUB(__imp__XamInputNonControllerGetRaw);
XAM_EXPORT_STUB(__imp__XamInputNonControllerGetRawEx);
XAM_EXPORT_STUB(__imp__XamInputNonControllerSetRaw);
XAM_EXPORT_STUB(__imp__XamInputNonControllerSetRawEx);
XAM_EXPORT_STUB(__imp__XamInputRawState);
XAM_EXPORT_STUB(__imp__XamInputResetLayoutKeyboard);
XAM_EXPORT_STUB(__imp__XamInputSendStayAliveRequest);
XAM_EXPORT_STUB(__imp__XamInputSendXenonButtonPress);
XAM_EXPORT_STUB(__imp__XamInputSetKeyLocks);
XAM_EXPORT_STUB(__imp__XamInputSetKeyboardTranslationHud);
XAM_EXPORT_STUB(__imp__XamInputSetLayoutKeyboard);
XAM_EXPORT_STUB(__imp__XamInputSetMinMaxAuthDelay);
XAM_EXPORT_STUB(__imp__XamInputSetTextMessengerIndicator);
XAM_EXPORT_STUB(__imp__XamInputToggleKeyLocks);

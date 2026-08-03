#pragma once

#include <cstdint>

namespace ac6::save_dialog {

// Bridges the guest's already-computed pad edge to the save dialog response
// word. Navigation remains owned and rendered by the guest; this only supplies
// the missing confirm result at the boundary consumed by the save state
// machine.
class InputBridge {
 public:
  uint32_t Observe(uint32_t edge, bool waiting, uint32_t dialog_type) {
    if (!waiting) {
      was_waiting_ = false;
      return 0;
    }
    if (!was_waiting_) {
      yes_selected_ = false;  // AC6 opens the two-button prompt on NO.
      was_waiting_ = true;
    }

    if (dialog_type == 6 || dialog_type == 30) {
      if (edge & 0x0004u) yes_selected_ = true;   // D-pad left: YES.
      if (edge & 0x0008u) yes_selected_ = false;  // D-pad right: NO.
      if (edge & 0x1000u) return yes_selected_ ? 1u : 2u;
    } else if ((dialog_type == 4 || dialog_type == 8 || dialog_type == 10 ||
                dialog_type == 35 || dialog_type == 37) &&
               (edge & 0x1000u)) {
      // The pre-save warning, file-create completion notices, and both
      // storage-create notices have one OK button and use response 1.
      return 1u;
    }
    return 0;
  }

 private:
  bool was_waiting_ = false;
  bool yes_selected_ = false;
};

// The file browser is drawn and updated by the guest. Its dedicated selector
// state machine (sub_821C3BE8) parks in state 3 until [screen+16] receives the
// one-based selected file. The host only supplies that missing selection on A;
// rendering and every subsequent transition remain guest-owned.
struct FileBrowserState {
  uint32_t outer_state;
  uint32_t selector_state;
  uint32_t selection_result;
  uint32_t response;
  uint32_t dialog_type;
  uint32_t result;
  uint32_t inner_state;
  uint32_t operation_state;
  uint32_t mode;
};

inline bool IsFileBrowserWaiting(const FileBrowserState& state) {
  return state.outer_state == 8 && state.selector_state == 3 &&
         state.selection_result == 0 && state.result == 0 &&
         state.inner_state == 0 && state.operation_state == 0 &&
         state.mode == 1;
}

inline uint32_t FileBrowserSelection(uint32_t edge,
                                     const FileBrowserState& state) {
  if (!IsFileBrowserWaiting(state)) return 0;
  if (edge & 0x1000u) return 1;  // A: FILE 01 is initially highlighted.
  // Native B already reaches sub_821C3BE8 through its keystroke path and
  // publishes the negative cancellation result. Do not duplicate it here.
  return 0;
}

}  // namespace ac6::save_dialog

#include "../ac6_save_dialog_input_bridge.h"

#include <cassert>

int main() {
  ac6::save_dialog::InputBridge bridge;

  assert(bridge.Observe(0x1000, false, 30) == 0);
  assert(bridge.Observe(0, true, 30) == 0);
  assert(bridge.Observe(0x1000, true, 30) == 2);

  assert(bridge.Observe(0, false, 0) == 0);
  assert(bridge.Observe(0x0004, true, 30) == 0);
  assert(bridge.Observe(0x1000, true, 30) == 1);
  assert(bridge.Observe(0x0008, true, 30) == 0);
  assert(bridge.Observe(0x1000, true, 30) == 2);

  assert(bridge.Observe(0, false, 0) == 0);
  assert(bridge.Observe(0x1000, true, 6) == 2);
  assert(bridge.Observe(0x0004, true, 6) == 0);
  assert(bridge.Observe(0x1000, true, 6) == 1);

  assert(bridge.Observe(0, false, 0) == 0);
  assert(bridge.Observe(0x1000, true, 4) == 1);

  assert(bridge.Observe(0, false, 0) == 0);
  assert(bridge.Observe(0x1000, true, 8) == 1);

  assert(bridge.Observe(0, false, 0) == 0);
  assert(bridge.Observe(0x1000, true, 10) == 1);

  assert(bridge.Observe(0, false, 0) == 0);
  assert(bridge.Observe(0x1000, true, 37) == 1);

  assert(bridge.Observe(0, false, 0) == 0);
  assert(bridge.Observe(0x1000, true, 35) == 1);

  const ac6::save_dialog::FileBrowserState file_waiting{
      8, 3, 0, 0, 0, 0, 0, 0, 1};
  assert(ac6::save_dialog::IsFileBrowserWaiting(file_waiting));
  assert(ac6::save_dialog::FileBrowserSelection(0x1000, file_waiting) == 1);
  assert(ac6::save_dialog::FileBrowserSelection(0x2000, file_waiting) == 0);
  assert(ac6::save_dialog::FileBrowserSelection(0x0008, file_waiting) == 0);

  // The no-storage path reaches the same retail selector state while retaining
  // the completed notice's response/type words. They are not selector guards:
  // sub_821C3BE8 state 3 waits on selection_result, not these stale fields.
  auto stale_notice = file_waiting;
  stale_notice.response = 1;
  stale_notice.dialog_type = 35;
  assert(ac6::save_dialog::IsFileBrowserWaiting(stale_notice));
  assert(ac6::save_dialog::FileBrowserSelection(0x1000, stale_notice) == 1);

  auto adjacent = file_waiting;
  adjacent.inner_state = 9;
  assert(!ac6::save_dialog::IsFileBrowserWaiting(adjacent));
  adjacent = file_waiting;
  adjacent.outer_state = 6;
  assert(!ac6::save_dialog::IsFileBrowserWaiting(adjacent));
  adjacent = file_waiting;
  adjacent.selector_state = 2;
  assert(!ac6::save_dialog::IsFileBrowserWaiting(adjacent));
  adjacent = file_waiting;
  adjacent.selection_result = 1;
  assert(ac6::save_dialog::FileBrowserSelection(0x1000, adjacent) == 0);
  return 0;
}

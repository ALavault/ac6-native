#include "ppc_recomp_shared.h"

// Link-only probe. It does not execute guest code or touch retail memory; the
// native runtime gate must provide the guest memory/import services first.
__attribute__((noinline)) PPCFunc* first_guest_function() {
  return &sub_82090000;
}

int main() {
  if (first_guest_function() == nullptr) return 1;
  if (PPCFuncMappings[0].guest != 0x82090000u ||
      PPCFuncMappings[0].host == nullptr) {
    return 2;
  }
  std::size_t count = 0;
  while (count < 20000u && PPCFuncMappings[count].guest != 0u) ++count;
  return count > 8000u ? 0 : 3;
}

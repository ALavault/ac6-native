#pragma once

#include <cstdint>

// P2.3 store watchpoint control surface. See ac6_ppc_store_hook.h for the
// mechanism. No-ops unless built with AC6RECOMP_WATCH_GUEST_STORES.
namespace ac6::stores {

#ifdef AC6RECOMP_WATCH_GUEST_STORES

void Arm(uint32_t edge_mask);
void FlushIfComplete();

// Note() is declared in ac6_ppc_store_hook.h, which the generated sources see
// through -include. It is deliberately not repeated here: the prelude must be
// the single definition point, or a translation unit could pick up the
// declaration without the macros and store without recording.

#else

inline void Arm(uint32_t) {}
inline void FlushIfComplete() {}

#endif  // AC6RECOMP_WATCH_GUEST_STORES

}  // namespace ac6::stores

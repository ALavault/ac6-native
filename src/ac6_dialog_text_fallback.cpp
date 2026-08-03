#include "ac6_dialog_text_fallback.h"

#include <atomic>
#include <chrono>
#include <cstring>

#include <rex/ppc/context.h>

namespace ac6::dialog_text {
namespace {

using Clock = std::chrono::steady_clock;
std::atomic<int64_t> g_last_seen_ns{0};

int64_t NowNs() {
  return std::chrono::duration_cast<std::chrono::nanoseconds>(
             Clock::now().time_since_epoch())
      .count();
}

}  // namespace

void ApplyFallback(PPCContext& ctx, uint8_t* base) {
  const uint32_t key_address = ctx.r5.u32;
  if (!key_address ||
      std::memcmp(base + key_address, kMissingGameDataKey.data(),
                  kMissingGameDataKey.size() + 1) != 0) {
    return;
  }

  g_last_seen_ns.store(NowNs(), std::memory_order_release);
}

bool ShouldDrawFallback() {
  constexpr int64_t kVisibilityWindowNs = 250'000'000;
  const int64_t seen = g_last_seen_ns.load(std::memory_order_acquire);
  return seen != 0 && NowNs() - seen <= kVisibilityWindowNs;
}

}  // namespace ac6::dialog_text

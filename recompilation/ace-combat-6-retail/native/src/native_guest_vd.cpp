#include "ac6/native_guest_vd.h"

#include "ac6/native_vulkan_device.h"

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace ac6::native {
namespace {

constexpr std::uint32_t kMinRingLog2 = 5u;
constexpr std::uint32_t kMaxRingLog2 = 17u;

void trace(const char* format, ...) noexcept {
  const char* enabled = std::getenv("AC6_NATIVE_VD_TRACE");
  if (enabled == nullptr || std::strcmp(enabled, "1") != 0) return;
  va_list arguments;
  va_start(arguments, format);
  std::vfprintf(stderr, format, arguments);
  va_end(arguments);
  std::fputc('\n', stderr);
}

std::uint32_t load_guest_word(const std::uint8_t* base,
                              GuestAddress address) noexcept {
  std::uint32_t value = 0u;
  std::memcpy(&value, base + address, sizeof(value));
  return __builtin_bswap32(value);
}

void store_guest_word(std::uint8_t* base, GuestAddress address,
                      std::uint32_t value) noexcept {
  const std::uint32_t encoded = __builtin_bswap32(value);
  std::memcpy(base + address, &encoded, sizeof(encoded));
}

// gpu_swap() emulates the Xenos GPU's own byte-lane swap unit: its result
// IS the final byte pattern the hardware would write to memory, already in
// the correct guest (big-endian) order once read back as raw bytes off a
// little-endian host. Unlike store_guest_word() -- which converts a normal
// host-native logical value into guest-BE bytes via one bswap -- writing a
// gpu_swap() result through store_guest_word() applies a second,
// compounding bswap and corrupts the delivered value. Use this instead for
// anything that has already passed through gpu_swap().
void store_guest_bytes_raw(std::uint8_t* base, GuestAddress address,
                           std::uint32_t already_swapped_value) noexcept {
  std::memcpy(base + address, &already_swapped_value,
              sizeof(already_swapped_value));
}

std::uint32_t gpu_swap(std::uint32_t value, Endian endian) noexcept {
  switch (endian) {
    case Endian::kNone:
      return value;
    case Endian::k8In16:
      return ((value & 0x00ff00ffu) << 8u) |
             ((value & 0xff00ff00u) >> 8u);
    case Endian::k8In32:
      return __builtin_bswap32(value);
    case Endian::k16In32:
      return (value << 16u) | (value >> 16u);
  }
  return value;
}

}  // namespace

void NativeGuestVdService::bind(std::uint8_t* base, MmioBus& bus,
                                 VdBridge& bridge, XenosState& state,
                                 VulkanBackend& backend) noexcept {
  std::lock_guard lock(mutex_);
  base_ = base;
  bus_ = &bus;
  bridge_ = &bridge;
  state_ = &state;
  backend_ = &backend;
  bridge_->set_guest_memory(base);
  ring_base_ = 0u;
  ring_guest_base_ = 0u;
  ring_size_ = 0u;
  readback_ = 0u;
  producer_object_ = 0u;
  ring_words_.clear();
  allocations_.clear();
  present_target_ = nullptr;
  poller_started_ = false;
  stop_poller_.store(false);
}

void NativeGuestVdService::unbind() noexcept {
  std::lock_guard lock(mutex_);
  stop_poller_.store(true);
  base_ = nullptr;
  bus_ = nullptr;
  bridge_ = nullptr;
  state_ = nullptr;
  backend_ = nullptr;
  present_target_ = nullptr;
  pinned_ = nullptr;
}

void NativeGuestVdService::bind_pinned(PinnedShaderRuntime* pinned) noexcept {
  std::lock_guard lock(mutex_);
  pinned_ = pinned;
}

void NativeGuestVdService::bind_offscreen(
    VulkanOffscreenTarget* target) noexcept {
  std::lock_guard lock(mutex_);
  present_target_ = target;
}

void NativeGuestVdService::register_allocation(
    std::uint8_t* base, GuestAddress guest_address,
    std::uint32_t bytes) noexcept {
  std::lock_guard lock(mutex_);
  if (base_ == nullptr || base != base_ || bytes == 0u ||
      guest_address > 0xffffffffu - bytes) return;
  const GuestAddress physical = guest_address & 0x1fffffffu;
  allocations_.push_back(Allocation{guest_address, physical, bytes});
  trace("vd alloc guest=0x%08x phys=0x%08x bytes=0x%x", guest_address,
        physical, bytes);
}

bool NativeGuestVdService::resolve_physical_locked(
    GuestAddress physical, GuestAddress& guest) const noexcept {
  for (auto it = allocations_.rbegin(); it != allocations_.rend(); ++it) {
    if (physical >= it->physical && physical - it->physical < it->bytes) {
      guest = it->guest + (physical - it->physical);
      return true;
    }
  }
  return false;
}

void NativeGuestVdService::initialize_ring(std::uint8_t* base,
                                            GuestAddress physical_base,
                                            std::uint32_t log2_size) noexcept {
  std::lock_guard lock(mutex_);
  if (base_ == nullptr || base != base_ || bus_ == nullptr ||
      log2_size < kMinRingLog2 || log2_size > kMaxRingLog2) {
    return;
  }
  const std::uint32_t shift = log2_size + 3u;
  const std::uint32_t bytes = 1u << shift;
  GuestAddress guest_base = 0u;
  if ((physical_base & 0xfffu) != 0u ||
      !resolve_physical_locked(physical_base, guest_base) ||
      !bus_->write(MmioBus::kRingBase, physical_base) ||
      !bus_->write(MmioBus::kRingSize, bytes)) {
    return;
  }
  trace("vd ring phys=0x%08x guest=0x%08x log2=%u bytes=0x%x", physical_base,
        guest_base, log2_size, bytes);
  ring_base_ = physical_base;
  ring_guest_base_ = guest_base;
  ring_size_ = bytes;
  ring_words_.assign(bytes / sizeof(std::uint32_t), 0u);
}

void NativeGuestVdService::enable_readback(
    std::uint8_t* base, GuestAddress physical_address) noexcept {
  std::lock_guard lock(mutex_);
  if (base_ == nullptr || base != base_ || ring_size_ == 0u) return;
  GuestAddress guest = 0u;
  if (resolve_physical_locked(physical_address, guest)) {
    readback_ = guest;
    trace("vd readback phys=0x%08x guest=0x%08x", physical_address, guest);
  } else {
    trace("vd readback unresolved phys=0x%08x", physical_address);
  }
  // The first primary command buffer is published by the qualified object
  // field before the guest reaches its first VdSwap call. Keep this watcher
  // bounded to that field (never scan ring contents); later VdSwap calls also
  // invoke the same exact publication routine.
  start_poller_locked();
}

void NativeGuestVdService::start_poller_locked() noexcept {
  if (poller_started_) return;
  poller_started_ = true;
  try {
    std::thread([this] { poll_loop(); }).detach();
  } catch (...) {
    poller_started_ = false;
  }
}

void NativeGuestVdService::poll_loop() noexcept {
  for (;;) {
    {
      std::lock_guard lock(mutex_);
      if (stop_poller_.load() || base_ == nullptr) return;
    }
    poll_once();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

bool NativeGuestVdService::discover_write_index_locked(
    std::uint32_t& write_index) noexcept {
  if (ring_size_ == 0u) return false;
  const std::uint32_t ring_words = ring_size_ / sizeof(std::uint32_t);
  // The qualified Vd object is carved out of the Xenon heap. It is therefore
  // not necessarily the beginning of an allocation. Once located, retain its
  // address and observe only its primary-ring producer field (+10952).
  if (producer_object_ != 0u) {
    const std::uint32_t value =
        load_guest_word(base_, producer_object_ + 10952u);
    if (value < ring_words) {
      write_index = value;
      return true;
    }
    producer_object_ = 0u;
  }
  const GuestAddress expected_state =
      readback_ >= 60u ? readback_ - 60u : 0u;
  if (expected_state == 0u) return false;
  for (const Allocation& allocation : allocations_) {
    if (allocation.bytes < 10912u) continue;
    const std::uint64_t limit =
        static_cast<std::uint64_t>(allocation.bytes) - 10912u;
    for (std::uint64_t offset = 0u; offset <= limit; offset += 16u) {
      const GuestAddress candidate =
          allocation.guest + static_cast<GuestAddress>(offset);
      const GuestAddress state = load_guest_word(base_, candidate + 10896u);
      if (state != expected_state) continue;
      const std::uint32_t value =
          load_guest_word(base_, candidate + 10952u);
      if (value >= ring_words) continue;
      producer_object_ = candidate;
      trace("vd discover object=0x%08x state=0x%08x write=%u cursor=0x%08x limit=0x%08x",
            candidate, state, value, load_guest_word(base_, candidate + 48u),
            load_guest_word(base_, candidate + 56u));
      write_index = value;
      return true;
    }
  }
  return false;
}

void NativeGuestVdService::poll_once() noexcept {
  std::lock_guard lock(mutex_);
  if (base_ == nullptr || bus_ == nullptr || bridge_ == nullptr ||
      state_ == nullptr || backend_ == nullptr || ring_size_ == 0u ||
      readback_ == 0u) {
    return;
  }
  std::uint32_t write_index = 0u;
  if (!discover_write_index_locked(write_index) ||
      write_index == bus_->ring_read() / sizeof(std::uint32_t)) {
    return;
  }
  trace("vd publish write=%u read=%u", write_index,
        bus_->ring_read() / sizeof(std::uint32_t));
  if (!bus_->write(MmioBus::kRingWrite, write_index * 4u)) return;
  drain_locked();
}

void NativeGuestVdService::drain_locked() noexcept {
  const std::size_t word_count = ring_size_ / sizeof(std::uint32_t);
  if (word_count == 0u || ring_guest_base_ > 0xffffffffu - ring_size_) return;
  for (std::size_t index = 0u; index < word_count; ++index) {
    ring_words_[index] = load_guest_word(
        base_, ring_guest_base_ + static_cast<GuestAddress>(index * 4u));
  }
  if (bus_->ring_read() == 0u) {
    trace("vd ring head=%08x %08x %08x %08x %08x %08x %08x %08x",
          ring_words_[0], ring_words_[1], ring_words_[2], ring_words_[3],
          ring_words_[4], ring_words_[5], ring_words_[6], ring_words_[7]);
  }
  bridge_->set_ring_words(ring_words_);
  std::vector<XenosCommand> commands;
  const DecodeResult result = bridge_->pump(*state_, commands);
  // r454: route real draws through the pinned real-renderer when bound,
  // instead of VulkanBackend's validate-only submit(). Guest memory is
  // synced into the runtime's shared-memory SSBO once per drain, before
  // any draw -- a full-range copy bounded by the SSBO's own size (512 MiB,
  // r438), not incremental. This is the naive first-cut approach: correct
  // (any in-window fetch address sees real guest bytes) but not optimized
  // for the copy cost. execute_frame() already resolves EDRAM on its own
  // PresentPacket handling, so the plain-clear present block below must be
  // skipped when this path is used, or present_count() double-counts
  // (the same double-present class r432 already fixed once for the
  // publish_write_address path).
  const bool use_pinned =
      pinned_ != nullptr && pinned_->valid() && present_target_ != nullptr;
  bool backend_accepted = false;
  // r454: only true once execute_frame() itself succeeds -- distinct from
  // use_pinned, which just says a pinned runtime is bound and eligible.
  // draw_pinned() fails closed on any draw state it doesn't have a pinned
  // shader variant for (a real, expected case -- the pinned registry is
  // 271 oracle-derived translations, not exhaustive, r255), and unlike
  // VulkanBackend::submit()'s pure structural validation, that failure is
  // about render *capability*, not packet validity. Treating it as an
  // overall decode rejection would stall the guest ring (readback never
  // advances below), a real regression VulkanBackend::submit() alone never
  // had -- so an unpinned draw falls back to plain structural validation
  // for this batch instead, preserving the pre-r454 accept/stall behavior
  // exactly, and only ever adding real rendering on top of it, never
  // taking it away.
  bool pinned_handled_present = false;
  if (result.ok()) {
    if (use_pinned) {
      const std::uint64_t sync_bytes = pinned_->shared_memory_dwords() * 4u;
      pinned_->write_shared_memory(
          0u, std::span<const std::uint8_t>(base_, sync_bytes));
      backend_accepted = pinned_->execute_frame(*present_target_, *state_, commands);
      if (backend_accepted) {
        pinned_handled_present = true;
      } else {
        trace("vd drain pinned execute_frame rejected, falling back to "
              "structural validation: %s", pinned_->error().c_str());
        backend_accepted = backend_->submit(*state_, commands);
      }
    } else {
      backend_accepted = backend_->submit(*state_, commands);
    }
  }
  if (!result.ok() || !backend_accepted) {
    if (!result.ok()) {
      trace("vd drain rejected decode_ok=0 code=%u offset=%zu detail=%s commands=%zu",
            static_cast<unsigned>(result.error.code), result.error.dword_offset,
            result.error.detail.c_str(), commands.size());
      const std::size_t read = bus_->ring_read() / 4u;
      const std::size_t words = std::min<std::size_t>(40u, ring_words_.size());
      if (read < ring_words_.size()) {
        const char* enabled = std::getenv("AC6_NATIVE_VD_TRACE");
        if (enabled != nullptr && std::strcmp(enabled, "1") == 0) {
          std::fprintf(stderr, "vd packet read=%zu", read);
          for (std::size_t index = 0u; index < words; ++index) {
            std::fprintf(stderr, " %08x",
                         ring_words_[(read + index) % ring_words_.size()]);
          }
          std::fputc('\n', stderr);
        }
      }
    } else {
      trace("vd drain rejected decode_ok=1 backend commands=%zu", commands.size());
    }
    return;
  }
  trace("vd drain accepted consumed_dwords=%zu", result.consumed);
  // r294 (diagnostic only): the "accepted" trace above never said WHAT was
  // accepted. r292/r293 found this ring goes silent after its first burst;
  // before assuming that burst is incomplete boot work, name what it
  // actually contained. One line per accepted batch (already bounded by
  // the same low frequency as "accepted" itself, not a new hot path).
  {
    std::size_t draw = 0u, resolve = 0u, present_count = 0u, wait = 0u,
                indirect = 0u, micro_init = 0u, event_write = 0u,
                immediate_shader = 0u;
    for (const XenosCommand& command : commands) {
      switch (command.index()) {
        case 0: ++draw; break;
        case 1: ++resolve; break;
        case 2: ++present_count; break;
        case 3: ++wait; break;
        case 4: ++indirect; break;
        case 5: ++micro_init; break;
        case 6: ++event_write; break;
        case 7: ++immediate_shader; break;
        default: break;
      }
    }
    trace("vd drain contents draw=%zu resolve=%zu present=%zu wait=%zu "
          "indirect=%zu micro_init=%zu event_write=%zu immediate_shader=%zu "
          "present_target_configured=%d",
          draw, resolve, present_count, wait, indirect, micro_init,
          event_write, immediate_shader, present_target_ != nullptr ? 1 : 0);
    // r435 (diagnostic only): r434 found PinnedShaderRuntime's shared-memory
    // SSBO is a 64 MiB window addressed directly by guest address. Before
    // deciding whether that window can hold real draws, name the real guest
    // addresses a real draw actually carries: DrawPacket.index_address (the
    // only guest address the packet itself carries -- vertex_address is
    // never populated from DRAW_INDX/DRAW_INDX_2, see native_xenos.cpp) and
    // the raw fetch-constant register block a real vertex/pixel shader
    // would read its vertex/texture fetch addresses from.
    if (draw != 0u) {
      for (const XenosCommand& command : commands) {
        const auto* drawpkt = std::get_if<DrawPacket>(&command);
        if (drawpkt == nullptr) continue;
        trace("vd draw index_address=0x%08x vertex_address=0x%08x "
              "vertex_count=%u index_count=%u",
              drawpkt->index_address, drawpkt->vertex_address,
              drawpkt->vertex_count, drawpkt->index_count);
      }
      constexpr std::uint32_t kRegShaderConstantFetch000 = 0x4800u;
      constexpr std::uint32_t kFetchConstantDwords = 192u;
      for (std::uint32_t dword = 0u; dword < kFetchConstantDwords; ++dword) {
        const std::uint32_t value =
            state_->register_value(kRegShaderConstantFetch000 + dword);
        if (value != 0u) {
          trace("vd fetch_const[%u]=0x%08x", dword, value);
        }
      }
    }
  }
  // r454: PresentPacket already resolved inside execute_frame() above when
  // pinned_handled_present -- redoing it here through the plain-clear path
  // would double-count present_count() (the same class of bug r432 fixed
  // for the publish_write_address path). Deliberately NOT gated on
  // use_pinned alone: a failed pinned attempt falls back to plain
  // structural validation above, and that fallback still needs this block
  // to actually present.
  if (present_target_ != nullptr && !pinned_handled_present) {
    for (const XenosCommand& command : commands) {
      const auto* present = std::get_if<PresentPacket>(&command);
      if (present == nullptr) continue;
      // Placeholder clear color until resolve-to-image lands: opaque black.
      if (!backend_->present_to_offscreen(*present_target_, *present, 0.0f,
                                          0.0f, 0.0f, 1.0f)) {
        trace("vd drain present failed surface=%u %ux%u: %s", present->surface,
              present->width, present->height, backend_->error().c_str());
        return;
      }
    }
  }
  if (readback_ != 0u) {
    store_guest_word(base_, readback_, bus_->ring_read() / 4u);
  }
  for (const XenosCommand& command : commands) {
    const auto* event = std::get_if<EventWriteShdPacket>(&command);
    if (event == nullptr) continue;
    GuestAddress guest_address = 0u;
    const GuestAddress physical = event->address & 0x1ffffffcu;
    if (!resolve_physical_locked(physical, guest_address)) {
      trace("vd event write unresolved phys=0x%08x", physical);
      continue;
    }
    const Endian endian = static_cast<Endian>(event->address & 3u);
    const std::uint32_t stored = gpu_swap(event->value, endian);
    store_guest_bytes_raw(base_, guest_address, stored);
    trace("vd event write guest=0x%08x raw_value=0x%08x endian=%u stored=0x%08x",
          guest_address, event->value, static_cast<unsigned>(endian), stored);
  }
}

void NativeGuestVdService::publish_write_address(
    std::uint8_t* base, GuestAddress guest_address) noexcept {
  std::lock_guard lock(mutex_);
  // r296 (diagnostic only): this is the ONLY call site VdSwap's stub
  // reaches. r295 showed zero "vd swap commit" traces ever fire below,
  // but that only proves zero SUCCESSFUL commits -- an early return in
  // either branch below (unbound service, or discover_write_index_locked
  // finding no new work) would silently no-op before reaching either
  // existing trace. VdSwap is a real per-frame kernel call, expected at
  // most tens of times/sec even in normal play -- unconditional trace
  // here is bounded by construction, not a hot-path risk like r282's.
  trace("vd swap entry secondary=0x%08x bound=%d readback=0x%08x",
        guest_address, base_ != nullptr && base == base_ ? 1 : 0, readback_);
  // r430: VdSwap is a genuine, separate kernel call on real Xbox 360
  // hardware -- it programs the display scanout directly, it is not a PM4
  // command the guest queues into the ring the code below drains. r427
  // found the guest never writes (and, per real hardware semantics, never
  // needs to write) an XE_SWAP packet itself; r429's plan to synthesize
  // and inject one into the ring at VdSwap's own cursor argument was
  // abandoned once this cycle found discover_write_index_locked() (right
  // below) never reads that cursor at all -- it reads the producer
  // object's own +10952 field, entirely independent of what VdSwap
  // receives. So there is no ring offset to get right: call the present
  // path directly instead of trying to inject a synthetic packet into an
  // unrelated stream (reports/ac6-retail-native-codegen-gate2-r430-*).
  // r432: gated on readback_ != 0u -- the same condition the branch below
  // uses to tell the real retail path from "the small, explicit unit-test/
  // API fixture" path (comment a few lines down). That fixture publishes a
  // real hand-built XE_SWAP packet through the ring/drain_locked() path on
  // purpose, to exercise decode+execute end to end; firing this direct
  // call unconditionally there double-presented (guest_vd_service_present_
  // executes_offscreen expects present_count()==2 after one packet, a
  // full clean rebuild caught present_count()==3 -- this fix was missing
  // the guard until then).
  if (base_ != nullptr && base == base_ && backend_ != nullptr &&
      present_target_ != nullptr && readback_ != 0u) {
    const PresentPacket packet{0u, present_target_->width(),
                               present_target_->height(), 0u};
    // r454: VdSwap's own direct present (r430) is the real per-frame swap
    // on this path -- resolve whatever the pinned runtime's EDRAM render
    // target already holds from this frame's ring-drained draws, instead
    // of just clearing, when a real renderer is bound.
    const bool use_pinned =
        pinned_ != nullptr && pinned_->valid() && state_ != nullptr;
    bool ok = false;
    std::uint64_t count = 0u;
    std::string_view err;
    if (use_pinned) {
      // r456: execute_frame()'s PresentPacket handling increments
      // present_count() unconditionally (even with no active EDRAM render
      // target -- there is simply nothing to resolve, per its own code),
      // so a "successful" pinned present is not proof the image was
      // actually touched. r455's capture diagnostic found exactly this:
      // when no pinned draw ever matched a shader this session (a real,
      // expected case, r454), the image stayed at its initial UNDEFINED
      // layout forever -- a real regression versus the old path, which
      // always cleared. Detect it via edram_resolves()'s own delta (the
      // one signal PinnedShaderRuntime already exposes for "did a resolve
      // actually happen") and fall back to a bare clear() -- not
      // backend_->present_to_offscreen(), which would double-count this
      // same present in diagnostics()'s now-additive backend_+pinned_ sum
      // (r455) -- only when nothing did.
      const std::uint64_t resolves_before = pinned_->edram_resolves();
      const std::vector<XenosCommand> one_cmd{packet};
      ok = pinned_->execute_frame(*present_target_, *state_, one_cmd);
      count = pinned_->present_count();
      err = pinned_->error();
      if (ok && pinned_->edram_resolves() == resolves_before) {
        if (!present_target_->clear(0.0f, 0.0f, 0.0f, 1.0f)) {
          trace("vd swap fallback clear failed: %s",
                present_target_->error().c_str());
        }
      }
    } else {
      ok = backend_->present_to_offscreen(*present_target_, packet, 0.0f,
                                          0.0f, 0.0f, 1.0f);
      count = backend_->present_count();
      err = backend_->error();
    }
    if (!ok) {
      trace("vd swap present failed %ux%u: %s", packet.width, packet.height,
            std::string(err).c_str());
    } else {
      trace("vd swap presented %ux%u present_count=%llu", packet.width,
            packet.height, static_cast<unsigned long long>(count));
    }
  }
  if (base_ == nullptr || base != base_ || bus_ == nullptr ||
      bridge_ == nullptr || state_ == nullptr || backend_ == nullptr ||
      ring_size_ == 0u || readback_ == 0u) {
    // Keep the small, explicit unit-test/API fixture usable when no qualified
    // RPtr writeback was installed: there the caller publishes a primary-ring
    // byte cursor directly. Retail VdSwap always has readback configured and
    // takes the object-field path below.
    if (base_ == nullptr || base != base_ || bus_ == nullptr ||
        bridge_ == nullptr || state_ == nullptr || backend_ == nullptr ||
        ring_size_ == 0u || readback_ != 0u ||
        guest_address < ring_guest_base_ ||
        guest_address - ring_guest_base_ >= ring_size_ ||
        ((guest_address - ring_guest_base_) & 3u) != 0u) {
      return;
    }
    const std::uint32_t write_index =
        (guest_address - ring_guest_base_) / sizeof(std::uint32_t);
    if (!bus_->write(MmioBus::kRingWrite, write_index * 4u)) return;
    drain_locked();
    return;
  }
  // VdSwap receives the completed secondary-buffer cursor (+4), not a
  // primary-ring address. The guest has already copied the staged packets and
  // advanced the qualified primary WPTR at +10952; commit that value exactly
  // at this publication edge.
  std::uint32_t write_index = 0u;
  const bool discovered = discover_write_index_locked(write_index);
  const std::uint32_t already_read = bus_->ring_read() / sizeof(std::uint32_t);
  // r296 (diagnostic only): names WHY the swap is a no-op -- distinguishes
  // "discovery failed entirely" from "discovery succeeded but the primary
  // WPTR the guest is supposed to have already advanced still matches what
  // was already committed" (the two early-return paths merged silently
  // above previously).
  trace("vd swap check secondary=0x%08x discovered=%d write_index=%u "
        "already_read=%u",
        guest_address, discovered ? 1 : 0, write_index, already_read);
  if (!discovered || write_index == already_read) {
    return;
  }
  trace("vd swap commit secondary=0x%08x primary_write=%u read=%u",
        guest_address, write_index, bus_->ring_read() / sizeof(std::uint32_t));
  if (!bus_->write(MmioBus::kRingWrite, write_index * 4u)) return;
  drain_locked();
}

NativeGuestVdService& native_guest_vd_service() noexcept {
  static NativeGuestVdService service;
  return service;
}

}  // namespace ac6::native

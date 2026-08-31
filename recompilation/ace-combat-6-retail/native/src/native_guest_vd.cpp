#include "ac6/native_guest_vd.h"

#include <algorithm>
#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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
  poller_started_ = false;
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
  if (!result.ok() || !backend_->submit(*state_, commands)) {
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
    store_guest_word(base_, guest_address, gpu_swap(event->value, endian));
  }
}

void NativeGuestVdService::publish_write_address(
    std::uint8_t* base, GuestAddress guest_address) noexcept {
  std::lock_guard lock(mutex_);
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
  if (!discover_write_index_locked(write_index) ||
      write_index == bus_->ring_read() / sizeof(std::uint32_t)) {
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

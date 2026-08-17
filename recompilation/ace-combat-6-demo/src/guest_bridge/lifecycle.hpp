// Internal GuestBridge domain fragment; included only by guest_bridge.cpp.
namespace ac6demo {

namespace {

constexpr std::uint32_t kPcrSize = 0xAB0U;
constexpr std::uint32_t kTlsBlockSize = 0x100U;
constexpr std::uint32_t kTebSize = 0x2E0U;
constexpr std::uint32_t kDefaultStackSize = 0x40000U;
constexpr std::uint32_t kGuestThreadId = 1U;

} // namespace

bool GuestBridge::available() const noexcept { return true; }

void GuestBridge::prepare(const ThreadImage &image) {
  if (prepared_) {
    return;
  }
  if (image.tls_address == 0U || image.tls_data_size == 0U ||
      image.tls_raw_size < image.tls_data_size || image.stack_size == 0U) {
    throw RuntimeTrap("qualified XEX has invalid guest thread metadata");
  }
  if (!memory_.mapped(image.tls_address, image.tls_data_size)) {
    throw RuntimeTrap("qualified XEX TLS template is not mapped", tick_, 0,
                      image.tls_address);
  }
  const auto tls_template =
      memory_.load_bytes(image.tls_address, image.tls_data_size);
  const auto stack_size = std::max(image.stack_size, kDefaultStackSize);
  const auto tls_block_size = std::max(kTlsBlockSize, image.tls_data_size);
  const auto thread_size = static_cast<std::uint64_t>(kPcrSize) +
                           tls_block_size + kTebSize + stack_size;
  if (thread_size > 0x01000000ULL) {
    throw RuntimeTrap("qualified guest thread image exceeds bootstrap mapping");
  }
  memory_.map_zero(kGuestThreadBase, static_cast<std::size_t>(thread_size));
  const auto tls_base = kGuestThreadBase + kPcrSize;
  const auto teb_base = tls_base + tls_block_size;
  const auto stack_top = teb_base + kTebSize + stack_size;
  stack_top_ = stack_top;
  if (tls_template.size() > tls_block_size) {
    throw RuntimeTrap("qualified XEX TLS template exceeds Xenon TLS block",
                      tick_, 0,
                      static_cast<std::uint32_t>(tls_template.size()));
  }
  memory_.store_u32(kGuestThreadBase + 0x000U, tls_base);
  memory_.store_u32(kGuestThreadBase + 0x100U, teb_base);
  memory_.store_u8(kGuestThreadBase + 0x10CU, 0U);
  memory_.store_u32(kGuestThreadBase + 0x150U, 0U);
  memory_.store_bytes(tls_base, tls_template);
  // This slot is consumed by the title's CRT/thread bootstrap before any
  // title-created TLS values exist. Its all-ones initializer is part of the
  // Xenon PCR/TLS contract, not a host-side success value.
  memory_.store_u32(tls_base + 0x10U, 0xFFFFFFFFU);
  memory_.store_u32(teb_base + 0x14CU, kGuestThreadId);
  const auto interrupt_stack = allocate_address(kDefaultStackSize);
  std::uint32_t interrupt_stack_page{};
  std::size_t interrupt_stack_bytes{};
  if (interrupt_stack == 0U ||
      !checked_page_range(interrupt_stack, kDefaultStackSize,
                          &interrupt_stack_page, &interrupt_stack_bytes)) {
    throw RuntimeTrap("cannot allocate guest graphics interrupt stack");
  }
  memory_.map_zero(interrupt_stack_page, interrupt_stack_bytes);
  record_allocation(interrupt_stack_page, interrupt_stack_bytes);
  graphics_interrupt_stack_top_ =
      interrupt_stack_page + static_cast<std::uint32_t>(interrupt_stack_bytes);
  memory_.map_mmio(
      0x7FC80714U, 4U,
      [this](std::uint32_t address, std::size_t length) -> std::uint64_t {
        if (address != 0x7FC80714U || length != 4U) {
          throw RuntimeTrap("unqualified Xenos MMIO read", tick_, 0, address);
        }
        return xenos_mmio_wptr_;
      },
      [this](std::uint32_t address, std::uint64_t value, std::size_t length) {
        if (address != 0x7FC80714U || length != 4U || value > 0xFFFFFFFFULL) {
          throw RuntimeTrap("unqualified Xenos MMIO write", tick_, 0, address);
        }
        xenos_mmio_wptr_ = static_cast<std::uint32_t>(value);
      });
  // PAL bytes at sub_82356510 read XMA register 0x600 before the first
  // XMACreateContext call. The pre-existing baseline exposed an empty
  // register (zero) and therefore reached the ordinal-548 trap later. Keep
  // that exact result when the experiment is disabled; only the opt-in route
  // allocates the context table and returns its wire-endian address. Xenon
  // uses lwbrx/stwbrx because the XMA aperture is little-endian on the wire.
  memory_.map_mmio(
      0x7FEA1800U, 4U,
      [this](std::uint32_t address, std::size_t length) -> std::uint64_t {
        if (address != 0x7FEA1800U || length != 4U) {
          throw RuntimeTrap("unqualified XMA context-array read", tick_, 0,
                            address);
        }
        const auto array = ensure_xma_context_array();
        if (array == 0U) {
          throw RuntimeTrap("cannot allocate XMA context array", tick_, 0,
                            address);
        }
        const auto wire_value = __builtin_bswap32(array);
        if (std::getenv("AC6_DEMO_WATCH_XMA_ADDRESS") != nullptr) {
          std::fprintf(stderr,
                       "AC6_XMA_ADDR_READ tick=%llu address=0x%08X "
                       "wire=0x%08X logical=0x%08X\n",
                       static_cast<unsigned long long>(tick_), address,
                       wire_value, array);
        }
        return wire_value;
      },
      [this](std::uint32_t address, std::uint64_t value, std::size_t length) {
        if (address != 0x7FEA1800U || length != 4U ||
            value > 0xFFFFFFFFULL) {
          throw RuntimeTrap("unqualified XMA context-array write", tick_, 0,
                            address);
        }
        if (std::getenv("AC6_DEMO_EXPERIMENTAL_XMA_CREATE") == nullptr) {
          xma_context_array_address_ =
              __builtin_bswap32(static_cast<std::uint32_t>(value));
          return;
        }
        throw RuntimeTrap("write to read-only XMA context-array register",
                          tick_, 0, address);
      });
  // Test-only bridge for the bounded PAL context-kick sequence. The static
  // PAL sequence is XMACreateContext -> MmGetPhysicalAddress -> stwbrx and
  // the observed values are wire-endian encodings of logical bits 0..3.
  // Xenia's generic XMA terminology is not promoted to the PAL product.
  // Keep this mapping absent from the default route and accept only the six
  // observed writes in an explicit experiment; reads and every other value
  // remain fail-closed.
  {
    memory_.map_mmio(
        0x7FEA1A80U, 4U,
        [this](std::uint32_t address, std::size_t length) -> std::uint64_t {
          throw RuntimeTrap("unqualified XMA kick register read", tick_, 0,
                            address ^ static_cast<std::uint32_t>(length));
        },
        [this](std::uint32_t address, std::uint64_t value,
               std::size_t length) {
          const auto expected_wire =
              static_cast<std::uint64_t>(xma_kick_expected_bit_) << 24U;
          // The kick is one-hot: the guest sets bit i for the i-th
          // context of the array this runtime handed out. The opt-in
          // experiment spelled that out as six absolute addresses
          // (0x2E800000 + i * 64) because that is where its array happened
          // to land; the array is allocated dynamically, so the rule is the
          // index, not the address. Everything else still traps.
          std::uint32_t expected_context = 0U;
          const auto array = xma_context_array_address_;
          if (array != 0U && xma_kick_expected_bit_ != 0U &&
              (xma_kick_expected_bit_ & (xma_kick_expected_bit_ - 1U)) == 0U) {
            const auto index =
                static_cast<std::uint32_t>(
                    std::countr_zero(xma_kick_expected_bit_));
            expected_context = array + index * 64U;
          }
          if (address != 0x7FEA1A80U || length != 4U ||
              value != expected_wire || xma_kick_expected_bit_ > 32U ||
              xma_last_physical_context_ != expected_context) {
            throw RuntimeTrap("unqualified XMA kick register write", tick_, 0,
                              address);
          }
          if (std::getenv("AC6_DEMO_WATCH_XMA_KICK") != nullptr) {
            std::fprintf(stderr,
                         "AC6_XMA_KICK tick=%llu thread=%u address=0x%08X "
                         "wire=0x%08X logical=0x%08X\n",
                         static_cast<unsigned long long>(tick_),
                         current_guest_thread_id, address,
                         static_cast<std::uint32_t>(value),
                         xma_kick_expected_bit_);
          }
          xma_kick_expected_bit_ <<= 1U;
          xma_last_physical_context_ = 0U;
        });
  }
  memory_.map_mmio(
      0x7FC86544U, 4U,
      [this](std::uint32_t address, std::size_t length) -> std::uint64_t {
        if (address != 0x7FC86544U || length != 4U) {
          throw RuntimeTrap("unqualified Xenos interrupt-status read", tick_, 0,
                            address);
        }
        // Xenos register index 0x1951. The SDK model and bounded PAL hardware
        // capture both report bit 0 set for every delivered vblank.
        return 1U;
      },
      [this](std::uint32_t address, std::uint64_t, std::size_t) {
        throw RuntimeTrap("write to read-only Xenos interrupt status", tick_, 0,
                          address);
      });
  const auto table_address =
      static_cast<std::uint64_t>(PPC_IMAGE_BASE) + PPC_IMAGE_SIZE;
  const auto table_size = static_cast<std::uint64_t>(PPC_CODE_SIZE) * 2ULL;
  if (table_address + table_size > kGuestMemoryBytes) {
    throw RuntimeTrap("generated indirect-call table exceeds guest memory");
  }
  memory_.map_zero(static_cast<std::uint32_t>(table_address),
                   static_cast<std::size_t>(table_size));
  auto *table = memory_.raw_base() + table_address;
  const auto &function_table = guest_function_table();
  if (!function_table.strictly_sorted) {
    throw RuntimeTrap("generated function mapping is not strictly sorted");
  }
  for (const auto *mapping = function_table.begin;
       mapping != function_table.begin + function_table.count; ++mapping) {
    if (mapping->guest < PPC_CODE_BASE ||
        mapping->guest >= PPC_CODE_BASE + PPC_CODE_SIZE) {
      throw RuntimeTrap(
          "generated function mapping outside configured code image", 0, 0,
          static_cast<std::uint32_t>(mapping->guest));
    }
    const auto offset = (mapping->guest - PPC_CODE_BASE) * 2ULL;
    std::memcpy(table + offset, &mapping->host, sizeof(mapping->host));
  }
  prepared_ = true;
}

void GuestBridge::set_tick(std::uint64_t tick) noexcept {
  tick_ = tick;
  input_.set_tick(tick);
  if (active_bridge == this) {
    update_guest_timers(*this);
  }
  AC6_PPC_SET_TICK(tick);
}

void GuestBridge::run_entry(std::uint32_t entry_point) {
  if (!prepared_) {
    throw RuntimeTrap("generated guest bridge was not prepared");
  }
  if (lookup_guest_function(entry_point) == nullptr) {
    throw RuntimeTrap("guest entry point is not in the qualified function map",
                      tick_, 0, entry_point);
  }
  if (primary_thread_.started && primary_thread_.entry != entry_point) {
    throw RuntimeTrap("guest entry point changed after fiber creation", tick_,
                      0, entry_point);
  }
  if (!primary_thread_.started) {
    primary_thread_.id = kPrimaryGuestThreadId;
    primary_thread_.entry = entry_point;
    primary_thread_.stack_top = stack_top_;
    primary_thread_.handle_open = false;
    primary_thread_.suspended = false;
    primary_thread_.finished = false;
  }
  active_bridge = this;
  const auto dispatch_graphics_interrupt = [&](std::uint32_t source) {
    auto *primary_fiber =
        static_cast<GuestFiber *>(primary_thread_.fiber_state);
    if (primary_fiber == nullptr || primary_fiber->ppc == nullptr ||
        graphics_interrupt_stack_top_ == 0U) {
      throw RuntimeTrap("graphics interrupt context is unavailable", tick_);
    }
    PPCContext interrupt = *primary_fiber->ppc;
    interrupt.r1.u32 = graphics_interrupt_stack_top_ - 0x100U;
    interrupt.r3.u32 = source;
    interrupt.r4.u32 = graphics_interrupt_context;
    interrupt.r5.u32 = 0U;
    interrupt.lr = 0U;
    if (interrupt.r13.u32 > std::numeric_limits<std::uint32_t>::max() - 268U ||
        !memory_.mapped(interrupt.r13.u32 + 268U, 1U)) {
      throw RuntimeTrap("graphics interrupt active-CPU field is unavailable",
                        tick_, 0, interrupt.r13.u32);
    }
    const auto active_cpu_address = interrupt.r13.u32 + 268U;
    const auto previous_active_cpu = memory_.load_u8(active_cpu_address);
    memory_.store_u8(active_cpu_address, 2U);
    const auto previous_thread_id = current_guest_thread_id;
    current_guest_thread_id = 2U;
    ac6demo::guest_bridge_detail::trace_graphics_interrupt_call(
        graphics_interrupt_callback, graphics_interrupt_context, source, tick_,
        current_guest_thread_id);
    try {
      AC6_PPC_CALL_INDIRECT(interrupt, memory_.raw_base(),
                            graphics_interrupt_callback);
    } catch (...) {
      memory_.store_u8(active_cpu_address, previous_active_cpu);
      current_guest_thread_id = previous_thread_id;
      throw;
    }
    memory_.store_u8(active_cpu_address, previous_active_cpu);
    current_guest_thread_id = previous_thread_id;
  };
  const auto dispatch_xaudio_frame = [&]() {
    auto *primary_fiber =
        static_cast<GuestFiber *>(primary_thread_.fiber_state);
    if (primary_fiber == nullptr || primary_fiber->ppc == nullptr ||
        graphics_interrupt_stack_top_ == 0U) {
      throw RuntimeTrap("XAudio callback context is unavailable", tick_);
    }
    PPCContext frame = *primary_fiber->ppc;
    frame.r1.u32 = graphics_interrupt_stack_top_ - 0x200U;
    frame.r3.u32 = xaudio_callback_context_;
    frame.r4.u32 = 0U;
    frame.r5.u32 = 0U;
    frame.lr = 0U;
    const auto previous_thread_id = current_guest_thread_id;
    current_guest_thread_id = 2U;
    try {
      AC6_PPC_CALL_INDIRECT(frame, memory_.raw_base(), xaudio_callback_);
    } catch (...) {
      current_guest_thread_id = previous_thread_id;
      throw;
    }
    current_guest_thread_id = previous_thread_id;
  };
  if (xenos_cp_interrupts_pending_ != 0U) {
    if (graphics_interrupt_callback == 0U) {
      throw RuntimeTrap("Xenos CP interrupt has no guest callback", tick_);
    }
    dispatch_graphics_interrupt(1U);
    --xenos_cp_interrupts_pending_;
    xenos_effects_.pending_interrupt_count = xenos_cp_interrupts_pending_;
  }
  if (graphics_interrupt_callback != 0U &&
      last_graphics_interrupt_tick_ != tick_) {
    dispatch_graphics_interrupt(0U);
    last_graphics_interrupt_tick_ = tick_;
  }
  // The registered render-driver client is guest code that this bridge has
  // stored since tick 106 and never once called, which is why the mixer
  // thread has waited on its eight auto-reset events ever since: nothing in
  // this runtime signals them, and on the console the audio driver does.
  //
  // Cadence is the XDK's native render frame, not a guess.  xauddefs.h gives
  // XAUDIOFRAMESIZE_NATIVE 256 and XAUDIOSAMPLERATE_NATIVE 48000, so a client
  // is driven 48000 / 256 = 187.5 times a second.  Simulation runs at 60 Hz,
  // so that is 25/8 frames per tick, emitted from an integer running total so
  // the schedule is exact and identical on replay: no host clock, no drift,
  // 3 or 4 calls per tick in a fixed pattern.
  //
  // What the callback does is the guest's business.  This only restores the
  // beat the hardware provides; it decides nothing about audio itself, and
  // no sample is decoded or submitted anywhere.
  // The registered callback takes no arguments. Its whole body is
  //     lis r11,-32098 ; lwz r3,-23256(r11) ; b 0x82355E58
  // so it loads a single global at 0x829DA528 and tail-calls with it. While
  // that global is null the callback dereferences null a few frames in, which
  // is exactly where driving it from the registration tick used to stop. The
  // guest publishes the pointer when its audio system is ready, so the
  // readiness condition is the guest's own, read from the address the
  // callback itself reads -- not a delay this runtime invents.
  //
  // Opt-in, and the reason is a measured negative. Driving it unconditionally
  // makes the guest fault on its FIRST frame, whenever that frame is: delayed
  // to tick 600 the trap simply moves to tick 600, identical registers. So the
  // object the audio path needs is never built by waiting, and the frames a
  // driven client would deliver cost the run 5,500 ticks of reach for no
  // compensating progress. Until that object is understood the default route
  // keeps its reach and this stays behind AC6_DEMO_EXPERIMENTAL_XAUDIO_DRIVE.
  constexpr std::uint32_t kXAudioClientStateGlobal = 0x829DA528U;
  const bool xaudio_client_ready =
      xaudio_callback_ != 0U &&
      std::getenv("AC6_DEMO_EXPERIMENTAL_XAUDIO_DRIVE") != nullptr &&
      memory_.mapped(kXAudioClientStateGlobal, 4U) &&
      memory_.load_u32(kXAudioClientStateGlobal) != 0U;
  if (xaudio_client_ready) {
    const auto target = ((tick_ + 1U) * 25U) / 8U;
    while (xaudio_frames_emitted_ < target) {
      dispatch_xaudio_frame();
      ++xaudio_frames_emitted_;
    }
  }
  (void)run_runnable_threads();
  resume_xenos_pending_batch();
}

} // namespace ac6demo

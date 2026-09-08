#include "ac6/native_guest_threads.h"
#include "ac6/native_pinned_shaders.h"
#include "ac6/native_runtime.h"

#if defined(AC6_NATIVE_GUEST_LINKED)
#include "ppc_recomp_shared.h"
#endif

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <thread>

namespace {

volatile bool g_probe_entry_requested = false;

#if defined(AC6_NATIVE_GUEST_LINKED)
// Xenon enters a title with r13 pointing at the processor-control region.
// The native entry probe has one deterministic guest thread, so reserve a
// small PCR/TLS pair outside the image, heap and probe stack ranges.
constexpr std::uint32_t kProbePcrAddress = 0x0f000000u;
constexpr std::uint32_t kProbeThreadAddress = 0x0f001000u;

void initialize_probe_thread(PPCContext& context, std::uint8_t* base,
                             std::uint32_t tls_address) {
  context.r13.u32 = kProbePcrAddress;
  // PCR fields used by the qualified early thread paths.
  PPC_STORE_U32(kProbePcrAddress + 0x00u, tls_address);
  PPC_STORE_U32(kProbePcrAddress + 0x30u, kProbePcrAddress);
  PPC_STORE_U32(kProbePcrAddress + 0x100u, kProbeThreadAddress);
  PPC_STORE_U32(kProbePcrAddress + 0x2a8u, kProbePcrAddress + 0x100u);
  PPC_STORE_U32(kProbePcrAddress + 0x10cu, 0u);  // processor 0
  PPC_STORE_U32(kProbePcrAddress + 0x70u, 0x8ff00000u);
  PPC_STORE_U32(kProbePcrAddress + 0x74u, 0x8f000000u);
  // Thread-local fields read by the early scheduler/timing path.
  PPC_STORE_U32(kProbeThreadAddress + 0x58u, 0u);
  PPC_STORE_U32(kProbeThreadAddress + 0x14cu, 1u);
  PPC_STORE_U32(kProbeThreadAddress + 0x150u, 0u);
  PPC_STORE_U8(kProbeThreadAddress + 0x73u, 0u);
}
#endif

void print_usage(std::ostream& stream, std::string_view program) {
  stream << "Usage: " << program << " <ISO|assets/>\n"
         << "       " << program << " --probe-entry <ISO|assets/>\n"
         << "       " << program << " --self-test\n";
}

int self_test() {
  // Keep this probe side-effect free: it validates the public media parser
  // without creating an XDG save directory or requiring retail bytes.
  const auto iso = ac6::native::parse_media_argument("missing.iso");
  // r255: the qualified US pinned shader registry must load and activate in
  // the product binary (271 oracle-translated fetches from
  // native/fixtures/pinned-shader-registry.v1.bin).
  const bool registry_ok = ac6::native::register_bundled_pinned_shader_registry() &&
                           ac6::native::ShaderTranslator::pinned_count() >= 271u;
  return (iso.has_value() || !registry_ok) ? EXIT_FAILURE : EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char** argv) {
  // r255: pin the qualified US shader registry before any guest execution.
  // Fail closed: without it the renderer has no qualified translation.
  if (!ac6::native::register_bundled_pinned_shader_registry()) {
    std::cerr << "pinned shader registry failed to load\n";
    return 1;
  }
  if (argc == 2 && std::string_view(argv[1]) == "--self-test") {
    return self_test();
  }
  // r239: probe_entry was previously kept in a single callee-saved register
  // (r12b) that the optimizer reused for other temporaries between boot and
  // the later gated entry dispatch. The later `if (probe_entry)` then read a
  // clobbered low byte and never entered the probe. Keep the decision in
  // a volatile global that survives the large stack frame's reuse.
  const int saved_argc = argc;
  char** const saved_argv = argv;
  g_probe_entry_requested = saved_argc == 3 && std::string_view(saved_argv[1]) == "--probe-entry";
  const bool probe_entry = g_probe_entry_requested;
  if (saved_argc != 2 && !probe_entry) {
    print_usage(std::cerr, saved_argc > 0 ? saved_argv[0] : "ac6recomp");
    return 2;
  }

  const char* allow_entry_probe = std::getenv("AC6_NATIVE_ALLOW_ENTRY_PROBE");
  if (probe_entry &&
      (allow_entry_probe == nullptr ||
       std::string_view(allow_entry_probe) != std::string_view("1"))) {
    std::cerr << "ac6recomp: entry probe requires AC6_NATIVE_ALLOW_ENTRY_PROBE=1\n";
    return 2;
  }
  const std::filesystem::path media(probe_entry ? saved_argv[2] : saved_argv[1]);
  auto runtime = ac6::native::NativeRuntime::open(media);
  if (!runtime) {
    std::cerr << "ac6recomp: expected an existing .iso or assets directory\n";
    return EXIT_FAILURE;
  }
  if (!runtime->boot()) {
    std::cerr << "ac6recomp: boot failed: "
              << runtime->diagnostics().error << '\n';
    return EXIT_FAILURE;
  }
  std::cout << "ac6recomp: native runtime booted media " << media << '\n';
  if (runtime->xex_metadata() != nullptr) {
    std::cout << "ac6recomp: mapped XEX image 0x" << std::hex
              << runtime->loaded_guest_image_size() << " bytes at 0x"
              << runtime->xex_metadata()->load_address << std::dec << '\n';
  }
#if defined(AC6_NATIVE_GUEST_LINKED)
  const std::uint64_t lookup_base =
      static_cast<std::uint64_t>(PPC_IMAGE_BASE) + PPC_IMAGE_SIZE;
  const std::uint64_t lookup_span = static_cast<std::uint64_t>(PPC_CODE_SIZE) * 2u;
  std::size_t populated = 0u;
  for (std::size_t index = 0u; PPCFuncMappings[index].guest != 0u; ++index) {
    const std::uint64_t guest = PPCFuncMappings[index].guest;
    if (guest < PPC_CODE_BASE || guest >= PPC_CODE_BASE + PPC_CODE_SIZE ||
        (guest - PPC_CODE_BASE) * 2u >= lookup_span) {
      std::cerr << "ac6recomp: generated mapping is outside lookup table\n";
      return EXIT_FAILURE;
    }
    auto** slot = reinterpret_cast<PPCFunc**>(
        runtime->guest_address_space().base() + lookup_base +
        (guest - PPC_CODE_BASE) * 2u);
    *slot = PPCFuncMappings[index].host;
    ++populated;
  }
  if (populated == 0u) {
    std::cerr << "ac6recomp: generated lookup table is empty\n";
    return EXIT_FAILURE;
  }
  std::cout << "ac6recomp: populated " << populated
            << " generated indirect-call mappings\n";
  PPCFunc* entry_function = nullptr;
  std::uint32_t entry_start = 0u;
  const std::uint32_t entry_point = runtime->xex_metadata()->entry_point;
  for (std::size_t index = 0u; PPCFuncMappings[index].guest != 0u; ++index) {
    if (PPCFuncMappings[index].guest > entry_point) break;
    entry_start = static_cast<std::uint32_t>(PPCFuncMappings[index].guest);
    entry_function = PPCFuncMappings[index].host;
  }
  if (entry_function == nullptr) {
    std::cerr << "ac6recomp: XEX entry point has no generated mapping\n";
    return EXIT_FAILURE;
  }
  std::cout << "ac6recomp: XEX entry 0x" << std::hex << entry_point
            << " resolves to generated function 0x" << entry_start << std::dec
            << '\n';
  // First ABI smoke call only. This generated function is a qualified neutral
  // leaf; real entry-point dispatch remains gated on SDK bindings and is not
  // replaced by this probe.
  PPCContext guest_context{};
  sub_8209C0B4(guest_context, runtime->guest_address_space().base());
  std::cout << "ac6recomp: generated guest ABI smoke call passed\n";
  // r239: probe_entry was clobbered in a register; use the volatile global
  // that survives the large stack frame. Read once to avoid a second volatile
  // load after iostream buffering.
  const bool dispatch_flag = g_probe_entry_requested;
  if (dispatch_flag) {
    std::atomic<bool> entry_returned{false};
    std::thread entry_thread([&]() {
      PPCContext thread_context{};
      thread_context.fpscr.loadFromHost();
      thread_context.r1.u32 = 0x8ff00000u;
      initialize_probe_thread(thread_context, runtime->guest_address_space().base(),
                              runtime->xex_metadata()->tls_address);
      runtime->bind_guest_vd();
      try {
        entry_function(thread_context, runtime->guest_address_space().base());
        std::cout << "ac6recomp: generated entry returned\n" << std::flush;
      } catch (const ac6::native::GuestThreadTerminated&) {
        // r422: this thread may have raised IRQL (KeRaiseIrqlToDpcLevel)
        // and been terminated before its matching KfLowerIrql -- release
        // whatever it still holds instead of orphaning the shared
        // g_dpc_level_mutex for every other thread forever (see
        // native_guest_threads.h and reports/ac6-retail-native-codegen-
        // gate2-r421-hang-thread-pinned-live-blocked-on-non-raii-global-
        // irql-mutex-20260908.md).
        ac6::native::release_residual_dpc_level();
        std::cout << "ac6recomp: generated entry terminated its own thread\n" << std::flush;
      } catch (...) {
        ac6::native::release_residual_dpc_level();
        std::cout << "ac6recomp: generated entry threw unknown exception\n" << std::flush;
      }
      entry_returned.store(true);
    });
    entry_thread.detach();
    // Let the guest run for a bounded window. The entry itself spawns
    // worker threads and may return quickly; those workers continue to
    // drive the ring even after entry returns, so wait the full window.
    // r240: bounded 3 s window (30x100 ms). r280: AC6_NATIVE_PROBE_WINDOW_MS
    // overrides the window for diagnostics (the default is unchanged); the
    // r280 codec drain alone needs more than 3 s on a complete stream.
    int window_slices = 30;
    if (const char* window_ms = std::getenv("AC6_NATIVE_PROBE_WINDOW_MS")) {
      const long parsed = std::strtol(window_ms, nullptr, 10);
      if (parsed > 0 && parsed <= 600000L) window_slices = static_cast<int>((parsed + 99) / 100);
    }
    for (int i = 0; i < window_slices; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    std::cout << "ac6recomp: probe window elapsed, checking diagnostics (entry_returned="
              << entry_returned.load() << ")\n" << std::flush;
    std::cout << "ac6recomp: presented_frames=" << runtime->diagnostics().presented_frames
              << " state=" << static_cast<int>(runtime->diagnostics().state) << "\n" << std::flush;
  }
#endif
  if (!runtime->shutdown()) {
    std::cerr << "ac6recomp: shutdown failed: "
              << runtime->diagnostics().error << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

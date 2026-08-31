#include "ac6/native_runtime.h"

#if defined(AC6_NATIVE_GUEST_LINKED)
#include "ppc_recomp_shared.h"
#endif

#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

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
  return iso.has_value() ? EXIT_FAILURE : EXIT_SUCCESS;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 2 && std::string_view(argv[1]) == "--self-test") {
    return self_test();
  }
  const bool probe_entry = argc == 3 && std::string_view(argv[1]) == "--probe-entry";
  if (argc != 2 && !probe_entry) {
    print_usage(std::cerr, argc > 0 ? argv[0] : "ac6recomp");
    return 2;
  }

  const char* allow_entry_probe = std::getenv("AC6_NATIVE_ALLOW_ENTRY_PROBE");
  if (probe_entry &&
      (allow_entry_probe == nullptr ||
       std::string_view(allow_entry_probe) != std::string_view("1"))) {
    std::cerr << "ac6recomp: entry probe requires AC6_NATIVE_ALLOW_ENTRY_PROBE=1\n";
    return 2;
  }
  const std::filesystem::path media(probe_entry ? argv[2] : argv[1]);
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
  if (probe_entry) {
    runtime->bind_guest_vd();
    guest_context = {};
    // Generated floating-point helpers preserve host exception masks through
    // FPSCR; zero-initializing the guest context would unmask SIGFPE on x86.
    guest_context.fpscr.loadFromHost();
    guest_context.r1.u32 = 0x8ff00000u;
    initialize_probe_thread(guest_context, runtime->guest_address_space().base(),
                            runtime->xex_metadata()->tls_address);
    entry_function(guest_context, runtime->guest_address_space().base());
    std::cout << "ac6recomp: generated entry returned\n";
  }
#endif
  if (!runtime->shutdown()) {
    std::cerr << "ac6recomp: shutdown failed: "
              << runtime->diagnostics().error << '\n';
    return EXIT_FAILURE;
  }
  return EXIT_SUCCESS;
}

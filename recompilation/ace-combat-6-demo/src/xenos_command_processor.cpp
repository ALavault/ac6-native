#include "ac6demo/xenos_command_processor.hpp"

#include "ac6demo/hash.hpp"
#include "ac6demo/runtime_error.hpp"

#include <algorithm>
#include <array>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>

namespace ac6demo {
namespace {

constexpr std::uint8_t kImLoadImmediate = 0x2BU;
constexpr std::uint8_t kRegisterRmw = 0x21U;
constexpr std::uint8_t kDrawIndx2 = 0x36U;
constexpr std::uint8_t kInvalidateState = 0x3BU;
constexpr std::uint8_t kWaitRegMem = 0x3CU;
constexpr std::uint8_t kConditionalWrite = 0x45U;
constexpr std::uint8_t kEventWrite = 0x46U;
constexpr std::uint8_t kMeInit = 0x48U;
constexpr std::uint8_t kSetBinMask = 0x50U;
constexpr std::uint8_t kSetBinSelect = 0x51U;
constexpr std::uint8_t kInterrupt = 0x54U;
constexpr std::uint8_t kEventWriteShaderDone = 0x58U;
constexpr std::uint8_t kSetBinMaskLo = 0x60U;
constexpr std::uint8_t kSetBinMaskHi = 0x61U;
constexpr std::uint8_t kSetBinSelectLo = 0x62U;
constexpr std::uint8_t kSetBinSelectHi = 0x63U;
constexpr std::uint8_t kXeSwap = 0x64U;
constexpr std::uint32_t kSwapSignature = 0x53574150U;
constexpr std::uint16_t kDcLutRwMode = 0x1921U;
constexpr std::uint16_t kDcLutRwIndex = 0x1922U;
constexpr std::uint16_t kDcLut30Color = 0x1925U;
constexpr std::uint16_t kDcLutWriteEnableMask = 0x1927U;
constexpr std::array<std::uint32_t, 6> kReachedFrontbufferFetch{
    0x8A000002U, 0x1374A006U, 0x0059E4FFU, 0x00001414U, 0x00000000U,
    0x00000200U};

[[nodiscard]] bool is_reached_frontbuffer_fetch(
    std::span<const std::uint32_t> fetch, std::uint32_t address,
    std::uint32_t width, std::uint32_t height) noexcept {
  if (fetch.size() != kReachedFrontbufferFetch.size() ||
      !std::equal(fetch.begin(), fetch.end(), kReachedFrontbufferFetch.begin())) {
    return false;
  }
  // The generic Xenos fetch layout gives these fields their only qualified
  // meanings here; the complete six-dword equality keeps all other fields
  // fail-closed instead of guessing from a partial decode.
  return (fetch[0] & 3U) == 2U && ((fetch[0] >> 22U) & 0x1FFU) == 40U &&
         (fetch[0] >> 31U) != 0U && (fetch[1] & 0x3FU) == 6U &&
         ((fetch[1] >> 6U) & 3U) == 0U &&
         ((fetch[1] >> 12U) & 0xFFFFFU) == (address >> 12U) &&
         (fetch[2] & 0x1FFFU) + 1U == width &&
         ((fetch[2] >> 13U) & 0x1FFFU) + 1U == height &&
         ((fetch[5] >> 9U) & 3U) == 1U;
}

[[nodiscard]] constexpr bool is_reached_opaque_register_value(
    std::uint32_t index, std::uint32_t value) noexcept {
  // Pinned Xenia/Xenos authority models type-0 writes to these otherwise
  // unnamed indices as ordinary register-file storage, with no special-case
  // effect. Admit only the exact four values captured from the qualified PAL
  // demo packet e4356ed3...7312; names and broader semantics remain unknown.
  constexpr std::array<std::uint32_t, 4> values{
      0xC0100000U, 0x07F00000U, 0xC0000000U, 0x00100000U};
  if (index >= 0x0A02U && index <= 0x0A05U) {
    return value == values[index - 0x0A02U];
  }
  // The only reached writes to the two unnamed registers embedded between
  // the qualified VGT group and PA_SC registers are zero initialization.
  const bool reached_zero_only = index == 0x2290U || index == 0x2291U ||
                                 (index >= 0x230BU && index <= 0x2311U) ||
                                 index == 0x2313U || index == 0x2314U;
  return reached_zero_only && value == 0U;
}

[[noreturn]] void trap(const char *reason) { throw RuntimeTrap(reason); }

[[nodiscard]] std::string shader_hash(std::span<const std::uint32_t> words) {
  Sha256 digest;
  for (const std::uint32_t word : words) {
    const std::array<std::byte, 4> bytes{static_cast<std::byte>(word >> 24U),
                                         static_cast<std::byte>(word >> 16U),
                                         static_cast<std::byte>(word >> 8U),
                                         static_cast<std::byte>(word)};
    digest.update(bytes);
  }
  const auto bytes = digest.finish();
  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (const std::byte byte : bytes) {
    output << std::setw(2)
           << static_cast<unsigned>(std::to_integer<std::uint8_t>(byte));
  }
  return output.str();
}

void require_count(std::size_t actual, std::size_t expected) {
  if (actual != expected) {
    trap("invalid Xenos packet count");
  }
}

[[nodiscard]] std::uint32_t
load_raw_dword(const XenosCommandProcessor::GuestBytes &bytes) noexcept {
  return std::to_integer<std::uint32_t>(bytes[0]) |
         (std::to_integer<std::uint32_t>(bytes[1]) << 8U) |
         (std::to_integer<std::uint32_t>(bytes[2]) << 16U) |
         (std::to_integer<std::uint32_t>(bytes[3]) << 24U);
}

[[nodiscard]] std::uint32_t gpu_swap(std::uint32_t value,
                                     std::uint32_t endian) {
  switch (endian) {
  case 0U:
    return value;
  case 1U:
    return ((value << 8U) & 0xFF00FF00U) | ((value >> 8U) & 0x00FF00FFU);
  case 2U:
    return ((value & 0x000000FFU) << 24U) | ((value & 0x0000FF00U) << 8U) |
           ((value & 0x00FF0000U) >> 8U) | ((value & 0xFF000000U) >> 24U);
  case 3U:
    return (value << 16U) | (value >> 16U);
  default:
    trap("invalid Xenos endian mode");
  }
}

[[nodiscard]] XenosCommandProcessor::GuestBytes
store_raw_dword(std::uint32_t value) noexcept {
  return {static_cast<std::byte>(value), static_cast<std::byte>(value >> 8U),
          static_cast<std::byte>(value >> 16U),
          static_cast<std::byte>(value >> 24U)};
}

[[nodiscard]] bool compare(std::uint32_t function, std::uint32_t value,
                           std::uint32_t reference) noexcept {
  switch (function) {
  case 0U:
    return false;
  case 1U:
    return value < reference;
  case 2U:
    return value <= reference;
  case 3U:
    return value == reference;
  case 4U:
    return value != reference;
  case 5U:
    return value >= reference;
  case 6U:
    return value > reference;
  case 7U:
    return true;
  default:
    return false;
  }
}

enum class PacketEffectOutcome { Unhandled, Complete, Pending };

template <typename ReadDword, typename StageWrite, typename WriteRegister>
PacketEffectOutcome execute_effect_packet(
    std::uint8_t opcode, std::span<const std::uint32_t> payload,
    std::size_t packet_start,
    std::array<std::uint32_t, kXenosRegisterCount> &registers,
    std::uint64_t &bin_mask, std::uint64_t &bin_select,
    XenosBatchResult &result, ReadDword &&read_dword, StageWrite &&stage_write,
    WriteRegister &&write_register) {
  const auto count = payload.size();
  switch (opcode) {
  case kRegisterRmw: {
    require_count(count, 3U);
    const auto destination = payload[0] & 0x1FFFU;
    const bool reached =
        (payload[0] & 0xC0000000U) == 0U &&
        ((destination == 0x0081U && payload[1] == 0xFFFFFFFFU &&
          payload[2] == 0x80010000U) ||
         (destination == 0x0082U && payload[1] == 0xFFFFFFFFU &&
          payload[2] == 0U) ||
         (destination == 0x1841U && payload[1] == 0xFFFFF8FFU &&
          payload[2] == 0U) ||
         (destination == 0x1E4EU && payload[1] == 0xFFFFFFFEU &&
          payload[2] == 0U));
    if (!reached)
      trap("unqualified Xenos register RMW");
    write_register(destination,
                   (registers[destination] & payload[1]) | payload[2]);
    ++result.effects.register_rmw;
    return PacketEffectOutcome::Complete;
  }
  case kWaitRegMem: {
    require_count(count, 5U);
    const bool memory = (payload[0] & 0x10U) != 0U;
    std::uint32_t value{};
    if (memory) {
      const bool reached = payload[0] == 0x13U && payload[3] == 0xFFFFFFFFU &&
                           payload[4] == 0x100U &&
                           ((payload[1] == 0x16AE2006U &&
                             (payload[2] == 0U || payload[2] == 1U)) ||
                            (payload[1] == 0x16AE2002U &&
                             (payload[2] == 0U || payload[2] == 4U)));
      if (!reached)
        trap("unqualified Xenos memory wait");
      value = read_dword(payload[1]);
    } else {
      const bool coher = payload[1] == kXenosCoherStatusHost &&
                         payload[2] == 0U && payload[3] == 0x80000000U &&
                         payload[4] == 8U;
      const bool reached =
          payload[0] == 3U &&
          (coher || (payload[1] == 0x1973U && payload[2] == 0U &&
                     payload[3] == 1U && payload[4] == 0x100U));
      if (!reached)
        trap("unqualified Xenos register wait");
      if (coher)
        registers[kXenosCoherStatusHost] = 0U;
      value = registers[payload[1]];
    }
    if (!compare(payload[0] & 7U, value & payload[3], payload[2])) {
      result.pending_wait = true;
      result.pending_wait_memory = memory;
      result.pending_wait_address = payload[1];
      result.pending_wait_observed = value;
      result.pending_wait_reference = payload[2];
      result.pending_wait_mask = payload[3];
      result.pending_wait_interval = payload[4];
      result.consumed_dwords = packet_start;
      return PacketEffectOutcome::Pending;
    }
    ++result.effects.wait_reg_mem;
    return PacketEffectOutcome::Complete;
  }
  case kConditionalWrite:
    require_count(count, 6U);
    if (payload[0] != 7U || payload[1] != kDcLut30Color ||
        payload[2] != registers[kDcLut30Color] || payload[3] != 0xFFFFFFFFU ||
        payload[4] != kDcLutRwIndex || payload[5] == 0U || payload[5] > 256U ||
        (payload[5] & 0xFFU) != (registers[kDcLutRwIndex] & 0xFFU)) {
      trap("unqualified Xenos conditional write");
    }
    write_register(payload[4], payload[5]);
    ++result.effects.conditional_write;
    return PacketEffectOutcome::Complete;
  case kEventWrite:
    require_count(count, 1U);
    if (payload[0] != 6U)
      trap("unqualified Xenos event initiator");
    write_register(kXenosVgtEventInitiator, payload[0] & 0x3FU);
    ++result.effects.event_write;
    return PacketEffectOutcome::Complete;
  case kMeInit:
    require_count(count, 18U);
    ++result.effects.micro_engine_init;
    return PacketEffectOutcome::Complete;
  case kInterrupt:
    require_count(count, 1U);
    if (payload[0] != 4U)
      trap("unqualified Xenos CPU interrupt mask");
    result.cpu_interrupts.push_back(2U);
    ++result.effects.interrupt;
    return PacketEffectOutcome::Complete;
  case kEventWriteShaderDone: {
    require_count(count, 3U);
    const bool reached =
        payload[0] == 3U &&
        (payload[1] == 0x16AE1006U || payload[1] == 0x16AE1002U ||
         payload[1] == 0x16A5A006U || payload[1] == 0x16A5A002U);
    if (!reached)
      trap("unqualified Xenos shader-done write");
    write_register(kXenosVgtEventInitiator, payload[0] & 0x3FU);
    stage_write(payload[1], payload[2]);
    ++result.effects.event_write_shader_done;
    return PacketEffectOutcome::Complete;
  }
  case kInvalidateState:
    require_count(count, 1U);
    ++result.effects.invalidate_state;
    return PacketEffectOutcome::Complete;
  case kSetBinMaskLo:
    require_count(count, 1U);
    bin_mask = (bin_mask & 0xFFFFFFFF00000000ULL) | payload[0];
    return PacketEffectOutcome::Complete;
  case kSetBinMaskHi:
    require_count(count, 1U);
    bin_mask = (bin_mask & 0xFFFFFFFFULL) |
               (static_cast<std::uint64_t>(payload[0]) << 32U);
    return PacketEffectOutcome::Complete;
  case kSetBinSelectLo:
    require_count(count, 1U);
    bin_select = (bin_select & 0xFFFFFFFF00000000ULL) | payload[0];
    return PacketEffectOutcome::Complete;
  case kSetBinSelectHi:
    require_count(count, 1U);
    bin_select = (bin_select & 0xFFFFFFFFULL) |
                 (static_cast<std::uint64_t>(payload[0]) << 32U);
    return PacketEffectOutcome::Complete;
  case kSetBinMask:
    require_count(count, 2U);
    bin_mask = (static_cast<std::uint64_t>(payload[0]) << 32U) | payload[1];
    return PacketEffectOutcome::Complete;
  case kSetBinSelect:
    require_count(count, 2U);
    bin_select = (static_cast<std::uint64_t>(payload[0]) << 32U) | payload[1];
    return PacketEffectOutcome::Complete;
  default:
    return PacketEffectOutcome::Unhandled;
  }
}

template <typename WriteRegister>
void execute_renderer_packet(
    std::uint8_t opcode, bool predicated,
    std::span<const std::uint32_t> payload,
    std::array<std::uint32_t, kXenosRegisterCount> &registers,
    std::string &vertex_shader, std::string &pixel_shader,
    XenosBatchResult &result, WriteRegister &&write_register) {
  const auto count = payload.size();
  if (opcode == kImLoadImmediate) {
    if (count < 2U || payload[0] > 1U) {
      trap("invalid Xenos immediate shader stage");
    }
    const auto start = static_cast<std::uint16_t>(payload[1] >> 16U);
    const auto size = static_cast<std::uint16_t>(payload[1]);
    if (start != 0U || size == 0U || count != 2U + size) {
      trap("invalid Xenos immediate shader size");
    }
    const auto stage =
        payload[0] == 0U ? XenosShaderStage::Vertex : XenosShaderStage::Pixel;
    std::string identity = shader_hash(payload.subspan(2U, size));
    (stage == XenosShaderStage::Vertex ? vertex_shader : pixel_shader) =
        identity;
    result.renderer_commands.emplace_back(
        XenosShaderLoadCommand{
            stage, start, size, std::move(identity),
            std::vector<std::uint32_t>(payload.begin() + 2U, payload.end())});
    return;
  }
  if (opcode == kDrawIndx2) {
    require_count(count, 1U);
    const auto initiator = payload[0];
    const auto primitive = static_cast<std::uint8_t>(initiator & 0x3FU);
    const auto source = static_cast<std::uint8_t>((initiator >> 6U) & 3U);
    const auto index_count = static_cast<std::uint16_t>(initiator >> 16U);
    const bool point =
        primitive == static_cast<std::uint8_t>(XenosPrimitive::PointList) &&
        index_count == 1U;
    const bool rectangle =
        primitive == static_cast<std::uint8_t>(XenosPrimitive::RectangleList) &&
        index_count == 3U;
    if (source != static_cast<std::uint8_t>(XenosIndexSource::AutoIndex) ||
        (!point && !rectangle)) {
      trap("unsupported Xenos draw shape");
    }
    if (vertex_shader.empty() || pixel_shader.empty()) {
      trap("Xenos draw has incomplete shader state");
    }
    write_register(kXenosVgtDrawInitiator, initiator);
    result.renderer_commands.emplace_back(XenosDrawCommand{
        static_cast<XenosPrimitive>(primitive), XenosIndexSource::AutoIndex,
        ((initiator >> 11U) & 1U) != 0U ? XenosIndexFormat::Uint32
                                        : XenosIndexFormat::Uint16,
        index_count, predicated, vertex_shader, pixel_shader,
        std::make_shared<XenosRegisterSnapshot>(registers)});
    return;
  }
  if (opcode != kXeSwap) {
    trap("unknown Xenos type-3 opcode");
  }
  require_count(count, 4U);
  if (predicated || payload[0] != kSwapSignature || payload[2] == 0U ||
      payload[3] == 0U) {
    trap("invalid Xenos presentation packet");
  }
  const std::span<const std::uint32_t> fetch(
      registers.data() + kXenosTextureFetch00, 6U);
  const auto address = fetch[1] & 0xFFFFF000U;
  const auto format = static_cast<std::uint8_t>(fetch[1] & 0x3FU);
  const bool tiled = (fetch[0] >> 31U) != 0U;
  if (address != payload[1] || format != 6U || !tiled || payload[2] != 1280U ||
      payload[3] != 720U ||
      !is_reached_frontbuffer_fetch(fetch, payload[1], payload[2], payload[3])) {
    trap("unqualified Xenos presentation resource");
  }
  result.renderer_commands.emplace_back(XenosPresentCommand{
      shader_hash(fetch), format, tiled, payload[2], payload[3], address});
}

void validate_batch_structure(std::span<const std::uint32_t> dwords,
                              std::uint64_t bin_mask,
                              std::uint64_t bin_select) {
  std::size_t cursor = 0U;
  while (cursor < dwords.size()) {
    const auto header = dwords[cursor++];
    const auto type = header >> 30U;
    if (type == 2U) {
      continue;
    }
    const std::size_t count = ((header >> 16U) & 0x3FFFU) + 1U;
    if (count > dwords.size() - cursor) {
      trap("truncated Xenos packet");
    }
    if (type == 0U) {
      const std::size_t base = header & 0x7FFFU;
      const bool one_register = (header & 0x8000U) != 0U;
      if (!one_register && base + count > kXenosRegisterCount) {
        trap("invalid Xenos type-0 packet");
      }
      cursor += count;
      continue;
    }
    if (type != 3U) {
      trap("unsupported Xenos packet type");
    }
    const auto payload = dwords.subspan(cursor, count);
    cursor += count;
    const auto opcode = static_cast<std::uint8_t>((header >> 8U) & 0x7FU);
    if ((header & 1U) != 0U && (bin_select & bin_mask) == 0U) {
      continue;
    }
    switch (opcode) {
    case kRegisterRmw:
    case kEventWriteShaderDone:
      require_count(count, 3U);
      break;
    case kWaitRegMem:
      require_count(count, 5U);
      break;
    case kConditionalWrite:
      require_count(count, 6U);
      break;
    case kEventWrite:
    case kInterrupt:
    case kInvalidateState:
    case kDrawIndx2:
    case kSetBinMaskLo:
    case kSetBinMaskHi:
    case kSetBinSelectLo:
    case kSetBinSelectHi:
      require_count(count, 1U);
      break;
    case kMeInit:
      require_count(count, 18U);
      break;
    case kSetBinMask:
    case kSetBinSelect:
      require_count(count, 2U);
      break;
    case kImLoadImmediate:
      if (count < 2U || payload[0] > 1U || (payload[1] >> 16U) != 0U ||
          (payload[1] & 0xFFFFU) == 0U ||
          count != 2U + (payload[1] & 0xFFFFU)) {
        trap("invalid Xenos immediate shader packet");
      }
      break;
    case kXeSwap:
      require_count(count, 4U);
      break;
    default:
      trap("unknown Xenos type-3 opcode");
    }
    if (opcode == kSetBinMaskLo)
      bin_mask = (bin_mask & 0xFFFFFFFF00000000ULL) | payload[0];
    if (opcode == kSetBinMaskHi)
      bin_mask = (bin_mask & 0xFFFFFFFFULL) |
                 (static_cast<std::uint64_t>(payload[0]) << 32U);
    if (opcode == kSetBinSelectLo)
      bin_select = (bin_select & 0xFFFFFFFF00000000ULL) | payload[0];
    if (opcode == kSetBinSelectHi)
      bin_select = (bin_select & 0xFFFFFFFFULL) |
                   (static_cast<std::uint64_t>(payload[0]) << 32U);
    if (opcode == kSetBinMask)
      bin_mask = (static_cast<std::uint64_t>(payload[0]) << 32U) | payload[1];
    if (opcode == kSetBinSelect)
      bin_select = (static_cast<std::uint64_t>(payload[0]) << 32U) | payload[1];
  }
}

} // namespace

XenosBatchResult XenosCommandProcessor::process_batch(
    const std::span<const std::uint32_t> dwords,
    const MemoryReadCallback &read_memory) {
  validate_batch_structure(dwords, bin_mask_, bin_select_);
  auto next_registers = registers_;
  std::uint64_t next_bin_mask = bin_mask_;
  std::uint64_t next_bin_select = bin_select_;
  std::string next_vertex_shader = vertex_shader_sha256_;
  std::string next_pixel_shader = pixel_shader_sha256_;
  auto next_gamma_lut = gamma_lut_;
  std::uint8_t next_gamma_component = gamma_lut_component_;
  XenosBatchResult result;

  const auto read_guest_dword = [&](std::uint32_t encoded_address) {
    const std::uint32_t address = encoded_address & ~std::uint32_t{3U};
    for (auto write = result.memory_writes.rbegin();
         write != result.memory_writes.rend(); ++write) {
      if (write->address == address) {
        return gpu_swap(load_raw_dword(write->guest_bytes),
                        encoded_address & 3U);
      }
    }
    if (!read_memory) {
      trap("Xenos memory callback unavailable");
    }
    const auto bytes = read_memory(address);
    if (!bytes) {
      trap("unmapped Xenos guest dword");
    }
    return gpu_swap(load_raw_dword(*bytes), encoded_address & 3U);
  };
  const auto stage_guest_write = [&](std::uint32_t encoded_address,
                                     std::uint32_t value) {
    const std::uint32_t address = encoded_address & ~std::uint32_t{3U};
    if (!read_memory || !read_memory(address)) {
      trap("unmapped Xenos guest dword write");
    }
    result.memory_writes.push_back(XenosGuestMemoryWrite{
        address, store_raw_dword(gpu_swap(value, encoded_address & 3U))});
  };
  const auto write_register = [&](std::uint32_t index, std::uint32_t value) {
    if (index >= next_registers.size()) {
      trap("invalid Xenos register write");
    }
    const bool reached_opaque_index =
        (index >= 0x0A02U && index <= 0x0A05U) || index == 0x2290U ||
        index == 0x2291U || (index >= 0x230BU && index <= 0x2311U) ||
        index == 0x2313U || index == 0x2314U;
    if (reached_opaque_index &&
        !is_reached_opaque_register_value(index, value)) {
      trap("unqualified Xenos register write");
    }
    next_registers[index] =
        index == kXenosCoherStatusHost ? value | 0x80000000U : value;
    constexpr std::uint32_t kScratchRegister0 = 0x0578U;
    constexpr std::uint32_t kScratchRegister7 = 0x057FU;
    constexpr std::uint32_t kScratchUpdateMask = 0x01DCU;
    constexpr std::uint32_t kScratchAddress = 0x01DDU;
    if (index >= kScratchRegister0 && index <= kScratchRegister7) {
      const auto scratch_index = index - kScratchRegister0;
      if ((next_registers[kScratchUpdateMask] & (1U << scratch_index)) != 0U) {
        const auto scratch_address = next_registers[kScratchAddress];
        if (next_registers[kScratchUpdateMask] != 0x00020033U ||
            (scratch_address != 0x16A5B000U &&
             scratch_address != 0x16AE2000U)) {
          trap("unqualified Xenos scratch writeback target");
        }
        // Xenos scratch writeback uses the CPU-visible big-endian layout.
        // Endian mode 2 produces those bytes through the shared GPU writer.
        stage_guest_write(scratch_address + scratch_index * 4U + 2U, value);
        ++result.effects.scratch_writeback;
      }
      return;
    }
    if (index == kDcLutRwIndex) {
      next_gamma_component = 0U;
      return;
    }
    if (index != kDcLut30Color) {
      return;
    }
    if (next_registers[kDcLutRwMode] != 0U ||
        next_registers[kDcLutWriteEnableMask] != 7U) {
      trap("unqualified Xenos gamma LUT mode");
    }
    const std::uint8_t lut_index =
        static_cast<std::uint8_t>(next_registers[kDcLutRwIndex]);
    next_gamma_lut[lut_index] = value & 0x3FFFFFFFU;
    next_gamma_component = 0U;
    next_registers[kDcLutRwIndex] =
        (next_registers[kDcLutRwIndex] & 0xFFFFFF00U) |
        static_cast<std::uint8_t>(lut_index + 1U);
  };
  const auto commit_state = [&] {
    registers_ = next_registers;
    bin_mask_ = next_bin_mask;
    bin_select_ = next_bin_select;
    vertex_shader_sha256_ = next_vertex_shader;
    pixel_shader_sha256_ = next_pixel_shader;
    gamma_lut_ = next_gamma_lut;
    gamma_lut_component_ = next_gamma_component;
  };

  std::size_t cursor = 0U;
  while (cursor < dwords.size()) {
    const std::size_t packet_start = cursor;
    const std::uint32_t header = dwords[cursor++];
    const std::uint32_t type = header >> 30U;
    if (type == 0U) {
      const std::size_t count = ((header >> 16U) & 0x3FFFU) + 1U;
      const std::size_t base = header & 0x7FFFU;
      const bool one_register = ((header >> 15U) & 1U) != 0U;
      if (count > dwords.size() - cursor ||
          (!one_register && base + count > next_registers.size())) {
        trap("invalid Xenos type-0 packet");
      }
      for (std::size_t index = 0; index < count; ++index) {
        write_register(
            static_cast<std::uint32_t>(one_register ? base : base + index),
            dwords[cursor++]);
      }
      continue;
    }
    if (type == 2U) {
      continue;
    }
    if (type != 3U) {
      trap("unsupported Xenos packet type");
    }

    const std::size_t count = ((header >> 16U) & 0x3FFFU) + 1U;
    if (count > dwords.size() - cursor) {
      trap("truncated Xenos type-3 packet");
    }
    const std::span<const std::uint32_t> payload =
        dwords.subspan(cursor, count);
    cursor += count;
    const std::uint8_t opcode =
        static_cast<std::uint8_t>((header >> 8U) & 0x7FU);
    const bool predicated = (header & 1U) != 0U;
    if (predicated && (next_bin_select & next_bin_mask) == 0U) {
      continue;
    }

    const auto effect = execute_effect_packet(
        opcode, payload, packet_start, next_registers, next_bin_mask,
        next_bin_select, result, read_guest_dword, stage_guest_write,
        write_register);
    if (effect == PacketEffectOutcome::Pending) {
      commit_state();
      return result;
    }
    if (effect == PacketEffectOutcome::Complete) {
      continue;
    }
    execute_renderer_packet(opcode, predicated, payload, next_registers,
                            next_vertex_shader, next_pixel_shader, result,
                            write_register);
  }

  result.consumed_dwords = dwords.size();
  commit_state();
  return result;
}

} // namespace ac6demo

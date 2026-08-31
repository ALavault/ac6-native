#pragma once

// Product-side Xenos boundary.
//
// This header intentionally has no ReXGlue, Xenia, Vulkan or generated-game
// dependency.  It describes the small, typed command boundary used by the
// native renderer.  The decoder is conservative: malformed or unknown input
// returns an error before changing state or appending a command.

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace ac6::native {

using GuestAddress = std::uint32_t;

enum class Endian : std::uint8_t {
  // Xenos GPU endian controls (the names and numeric values match the
  // hardware register encoding, not the host CPU byte order).
  kNone = 0,
  k8In16 = 1,
  k8In32 = 2,
  k16In32 = 3,
};

constexpr std::uint32_t load_word(std::span<const std::uint8_t> bytes,
                                  std::size_t offset, Endian endian) noexcept {
  if (offset > bytes.size() || bytes.size() - offset < 4u) return 0u;
  const std::uint8_t a = bytes[offset + 0u];
  const std::uint8_t b = bytes[offset + 1u];
  const std::uint8_t c = bytes[offset + 2u];
  const std::uint8_t d = bytes[offset + 3u];
  switch (endian) {
    case Endian::kNone:
      return static_cast<std::uint32_t>(a) |
             (static_cast<std::uint32_t>(b) << 8u) |
             (static_cast<std::uint32_t>(c) << 16u) |
             (static_cast<std::uint32_t>(d) << 24u);
    case Endian::k8In16:
      return static_cast<std::uint32_t>(b) |
             (static_cast<std::uint32_t>(a) << 8u) |
             (static_cast<std::uint32_t>(d) << 16u) |
             (static_cast<std::uint32_t>(c) << 24u);
    case Endian::k8In32:
      return static_cast<std::uint32_t>(d) |
             (static_cast<std::uint32_t>(c) << 8u) |
             (static_cast<std::uint32_t>(b) << 16u) |
             (static_cast<std::uint32_t>(a) << 24u);
    case Endian::k16In32:
      return static_cast<std::uint32_t>(c) |
             (static_cast<std::uint32_t>(d) << 8u) |
             (static_cast<std::uint32_t>(a) << 16u) |
             (static_cast<std::uint32_t>(b) << 24u);
  }
  return 0u;
}

struct GuestRange final {
  GuestAddress address{};
  std::uint32_t length{};

  [[nodiscard]] constexpr bool contains(GuestAddress value) const noexcept {
    return value >= address && value - address < length;
  }
};

struct DrawPacket final {
  std::uint32_t primitive_type{};
  std::uint32_t vertex_count{};
  std::uint32_t index_count{};
  GuestAddress vertex_address{};
  GuestAddress index_address{};
  std::uint32_t vertex_stride{};
  std::uint32_t index_format{};
  std::uint32_t state_generation{};
  // Optional state requirements set by the register translator. Zero means
  // no texture/feature dependency in this normalized packet.
  std::uint32_t texture_dimension{}; // 1=2D, 2=3D, 3=cube
  bool uses_blend_scissor{};
  bool uses_primitive_restart{};
};

struct ResolvePacket final {
  std::uint32_t source_edram{};
  std::uint32_t destination_surface{};
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t sample_count{1};
  Endian color_endian{Endian::kNone};
  bool depth{};
};

struct PresentPacket final {
  std::uint32_t surface{};
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint64_t sequence{};
};

struct WaitPacket final {
  std::uint32_t selector{};
};

struct IndirectBufferPacket final {
  GuestAddress address{};
  std::uint32_t dword_count{};
};

// PM4_ME_INIT is a fixed-size micro-engine bootstrap packet.  The native
// backend validates and consumes it as state, but never executes guest
// microcode on the host.
struct MicroEngineInitPacket final {
  std::array<std::uint32_t, 18> payload{};
};

struct EventWriteShdPacket final {
  std::uint32_t initiator{};
  GuestAddress address{};
  std::uint32_t value{};
};

struct ImmediateShaderPacket final {
  std::uint32_t shader_type{};
  std::uint32_t start{};
  std::uint32_t dword_count{};
};

using XenosCommand = std::variant<DrawPacket, ResolvePacket, PresentPacket,
                                  WaitPacket, IndirectBufferPacket,
                                  MicroEngineInitPacket, EventWriteShdPacket,
                                  ImmediateShaderPacket>;

class XenosState final {
 public:
  // TYPE0's base-register field (native_xenos.cpp's low_register_of()) is
  // 15 bits (`header & 0x7FFFu`), so 0x8000 is the full range that format
  // can ever address -- not a guessed hardware limit, but the complete
  // space TYPE0 packets can name. r95/r96: a real retail indirect buffer
  // (2840 dwords at guest 0x125c0000) was found writing register 0x5002,
  // rejected by the previous 0x4000 bound; parsing that buffer's full
  // TYPE0 stream found no register above 0x5002, so 0x8000 covers it with
  // headroom for content this session hasn't captured yet.
  static constexpr std::size_t kRegisterCount = 0x8000;
  static constexpr std::uint32_t kMaxEdramBytes = 0xA00000;

  explicit XenosState(std::uint32_t edram_bytes = kMaxEdramBytes) noexcept;

  [[nodiscard]] bool set_register(std::uint32_t index,
                                  std::uint32_t value) noexcept;
  [[nodiscard]] std::uint32_t register_value(std::uint32_t index) const noexcept;
  [[nodiscard]] std::uint32_t generation() const noexcept { return generation_; }
  [[nodiscard]] std::uint32_t edram_bytes() const noexcept { return edram_bytes_; }

  // Register snapshots make packet validation deterministic and testable.  A
  // packet decoder copies state, validates the copy, then commits it.
  [[nodiscard]] const std::array<std::uint32_t, kRegisterCount>& registers()
      const noexcept {
    return registers_;
  }

 private:
  std::array<std::uint32_t, kRegisterCount> registers_{};
  std::uint32_t edram_bytes_{};
  std::uint32_t generation_{};
};

enum class Pm4ErrorCode : std::uint8_t {
  kNone,
  kTruncatedHeader,
  kTruncatedPacket,
  kInvalidType,
  kInvalidCount,
  kInvalidRegister,
  kUnsupportedOpcode,
  kInvalidPayload,
  kInvalidRange,
  kInvalidWait,
  kRingCorrupt,
};

struct Pm4Error final {
  Pm4ErrorCode code{Pm4ErrorCode::kNone};
  std::size_t dword_offset{};
  std::string detail;

  [[nodiscard]] explicit operator bool() const noexcept {
    return code != Pm4ErrorCode::kNone;
  }
};

struct DecodeResult final {
  std::size_t consumed{};
  Pm4Error error{};

  [[nodiscard]] bool ok() const noexcept { return !error; }
};

// Xbox 360 PM4 envelope values used by the native capsule format.  Opcode
// numbers are kept in one place so capsule producers and the product agree.
namespace pm4 {
constexpr std::uint32_t kType0 = 0u;
constexpr std::uint32_t kType1 = 1u;
constexpr std::uint32_t kType2 = 2u;
constexpr std::uint32_t kType3 = 3u;

constexpr std::uint32_t kOpcodeWaitForIdle = 0x26u;
constexpr std::uint32_t kOpcodeWaitRegMem = 0x3Cu;
constexpr std::uint32_t kOpcodeWaitRegEq = 0x52u;
constexpr std::uint32_t kOpcodeRegRmw = 0x21u;
constexpr std::uint32_t kOpcodeInvalidateState = 0x3Bu;
constexpr std::uint32_t kOpcodeDrawIndx = 0x22u;
constexpr std::uint32_t kOpcodeDrawIndx2 = 0x36u;
constexpr std::uint32_t kOpcodeSetConstant = 0x2Du;
constexpr std::uint32_t kOpcodeInterrupt = 0x54u;
constexpr std::uint32_t kOpcodeContextUpdate = 0x5Eu;
constexpr std::uint32_t kOpcodeXeSwap = 0x64u;
constexpr std::uint32_t kOpcodeIndirectBuffer = 0x3Fu;
constexpr std::uint32_t kOpcodeMeInit = 0x48u;
constexpr std::uint32_t kOpcodeEventWriteShd = 0x58u;
constexpr std::uint32_t kOpcodeNop = 0x10u;
constexpr std::uint32_t kOpcodeImLoadImmediate = 0x2Bu;
constexpr std::uint32_t kOpcodeSetBinMaskLo = 0x60u;
constexpr std::uint32_t kOpcodeSetBinMaskHi = 0x61u;
constexpr std::uint32_t kOpcodeSetBinSelectLo = 0x62u;
constexpr std::uint32_t kOpcodeSetBinSelectHi = 0x63u;
constexpr std::uint32_t kOpcodeSetBinMask = 0x50u;
constexpr std::uint32_t kOpcodeSetBinSelect = 0x51u;
// r98: verified against real retail construction code (sub_821EB8B8,
// called from sub_821F00C0), not guessed. Writes a fixed 6-dword shape 256
// times, packing three parallel caller-supplied uint16 arrays; the source
// arrays are generated by dividing a loop counter by 127/255 into
// quotient/remainder pairs -- a linear-ramp/quantization-table shape,
// consistent with (not proven identical to) a display gamma or dither
// table upload. Treated like the existing SET_BIN_MASK/SELECT family:
// accepted structurally, payload not semantically modeled.
constexpr std::uint32_t kOpcodeSetGammaOrDitherTable = 0x45u;
// r98: verified against real retail construction code (sub_821EBB40).
// Single payload dword, gated by dirty-state-bit checks on the same
// object fields (+0x2abd/+0x2abe/+0x2940) and reached via the identical
// cursor/limit-overflow -> sub_821E60A8 pattern already characterized for
// the ring-packet-writer family (r93/r94) -- structurally the same shape
// as the already-implemented INVALIDATE_STATE (0x3B), for a different
// state group. Treated identically: accepted, no further state effect
// modeled.
constexpr std::uint32_t kOpcodeInvalidateStateExtended = 0x46u;
constexpr std::uint32_t kSwapSignature = 0x53574150u; // fourcc("SWAP")

constexpr std::uint32_t header(std::uint32_t type, std::uint32_t count,
                               std::uint32_t low = 0u) noexcept {
  if (type == kType1) return (type << 30u) | (low & 0x3FFFFFFFu);
  return (type << 30u) | ((count - 1u) << 16u) | low;
}

constexpr std::uint32_t type1_header(std::uint32_t index_1,
                                     std::uint32_t index_2) noexcept {
  return (kType1 << 30u) | ((index_2 & 0x7FFu) << 11u) |
         (index_1 & 0x7FFu);
}

constexpr std::uint32_t type3_header(std::uint32_t opcode,
                                     std::uint32_t payload_count) noexcept {
  return header(kType3, payload_count, (opcode & 0x7Fu) << 8u);
}
}  // namespace pm4

class Pm4Decoder final {
 public:
  // Decode one complete packet at offset zero.  State/output are untouched on
  // error, including when validation fails after reading the final payload.
  [[nodiscard]] static DecodeResult decode_one(
      std::span<const std::uint32_t> words, XenosState& state,
      std::vector<XenosCommand>& output);

  // Decode a bounded stream.  Earlier complete packets may commit; the packet
  // that fails remains wholly unapplied and is named by dword offset.
  [[nodiscard]] static DecodeResult decode_stream(
      std::span<const std::uint32_t> words, XenosState& state,
      std::vector<XenosCommand>& output);
};

class MmioBus final {
 public:
  static constexpr GuestAddress kRingBase = 0x7ED00000u;
  static constexpr GuestAddress kRingSize = 0x7ED00004u;
  static constexpr GuestAddress kRingRead = 0x7ED00008u;
  static constexpr GuestAddress kRingWrite = 0x7ED0000Cu;
  static constexpr GuestAddress kInterrupt = 0x7ED00010u;

  using InterruptCallback = std::function<void(std::uint32_t)>;

  explicit MmioBus(InterruptCallback callback = {});

  [[nodiscard]] bool write(GuestAddress address, std::uint32_t value);
  [[nodiscard]] bool read(GuestAddress address, std::uint32_t& value) const noexcept;
  [[nodiscard]] const Pm4Error& error() const noexcept { return error_; }
  [[nodiscard]] std::uint32_t ring_base() const noexcept { return ring_base_; }
  [[nodiscard]] std::uint32_t ring_size() const noexcept { return ring_size_; }
  [[nodiscard]] std::uint32_t ring_read() const noexcept { return ring_read_; }
  [[nodiscard]] std::uint32_t ring_write() const noexcept { return ring_write_; }
  void clear_error() noexcept { error_ = {}; }

 private:
  bool fail(Pm4ErrorCode code, const char* detail);

  InterruptCallback callback_;
  std::uint32_t ring_base_{};
  std::uint32_t ring_size_{};
  std::uint32_t ring_read_{};
  std::uint32_t ring_write_{};
  Pm4Error error_{};
};

class VdBridge final {
 public:
  explicit VdBridge(MmioBus& bus) noexcept : bus_(bus) {}

  void set_ring_words(std::span<const std::uint32_t> words) {
    ring_words_.assign(words.begin(), words.end());
  }
  void set_guest_words(GuestAddress base,
                       std::span<const std::uint32_t> words) {
    guest_memory_ = nullptr;
    guest_base_ = base;
    guest_words_.assign(words.begin(), words.end());
  }
  void set_guest_memory(std::uint8_t* base) noexcept {
    guest_memory_ = base;
    guest_base_ = 0u;
    guest_words_.clear();
  }
  [[nodiscard]] DecodeResult pump(XenosState& state,
                                  std::vector<XenosCommand>& output);
  [[nodiscard]] const Pm4Error& error() const noexcept { return error_; }

 private:
  MmioBus& bus_;
  std::vector<std::uint32_t> ring_words_{};
  std::uint8_t* guest_memory_{};
  GuestAddress guest_base_{};
  std::vector<std::uint32_t> guest_words_{};
  Pm4Error error_{};
};

}  // namespace ac6::native

#include "ac6/native_xenos.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <limits>

namespace ac6::native {

namespace {

constexpr std::uint32_t type_of(std::uint32_t header) noexcept {
  return header >> 30u;
}

constexpr std::uint32_t count_of(std::uint32_t header) noexcept {
  return ((header >> 16u) & 0x3FFFu) + 1u;
}

constexpr std::uint32_t low_register_of(std::uint32_t header) noexcept {
  return header & 0x7FFFu;
}

constexpr std::uint32_t opcode_of(std::uint32_t header) noexcept {
  return (header >> 8u) & 0x7Fu;
}

constexpr bool one_reg_of(std::uint32_t header) noexcept {
  return (header & 0x8000u) != 0u;
}

constexpr std::uint32_t type1_reg_a(std::uint32_t header) noexcept {
  return header & 0x7FFu;
}

constexpr std::uint32_t type1_reg_b(std::uint32_t header) noexcept {
  return (header >> 11u) & 0x7FFu;
}

Pm4Error make_error(Pm4ErrorCode code, std::size_t offset,
                    const char* detail) {
  return Pm4Error{code, offset, detail};
}

// std::to_string() formats decimals; every caller here labels the result
// with a literal "0x" prefix, so a decimal value under that prefix reads as
// a plausible-looking but wrong hex address (r94 found this made a genuine
// in-range guest address, 0x125c0000, print as the decimal-as-hex-digits
// "0x308019200" -- appearing to exceed 32 bits when it did not).
std::string to_hex(std::uint32_t value) {
  char buffer[11];
  std::snprintf(buffer, sizeof(buffer), "0x%08x", value);
  return std::string(buffer);
}

bool valid_dimensions(std::uint32_t width, std::uint32_t height) noexcept {
  return width != 0u && height != 0u && width <= 4096u && height <= 4096u;
}

bool valid_primitive(std::uint32_t primitive) noexcept {
  return (primitive >= 1u && primitive <= 8u) ||
         (primitive >= 0x0Cu && primitive <= 0x12u);
}

}  // namespace

XenosState::XenosState(std::uint32_t edram_bytes) noexcept
    : edram_bytes_(std::clamp(edram_bytes, 1u, kMaxEdramBytes)) {}

bool XenosState::set_register(std::uint32_t index,
                              std::uint32_t value) noexcept {
  if (index >= kRegisterCount) return false;
  registers_[index] = value;
  ++generation_;
  return true;
}

std::uint32_t XenosState::register_value(std::uint32_t index) const noexcept {
  return index < kRegisterCount ? registers_[index] : 0u;
}

DecodeResult Pm4Decoder::decode_one(std::span<const std::uint32_t> words,
                                    XenosState& state,
                                    std::vector<XenosCommand>& output) {
  if (words.empty()) {
    return {0u, make_error(Pm4ErrorCode::kTruncatedHeader, 0u,
                           "PM4 header is missing")};
  }
  const std::uint32_t header = words[0];
  const std::uint32_t type = type_of(header);
  const std::uint32_t count = type_of(header) == pm4::kType1 ? 2u : count_of(header);
  if (count == 0u) {
    return {0u, make_error(Pm4ErrorCode::kInvalidCount, 0u,
                           "PM4 packet count overflow")};
  }

  // Work on copies.  No state or output is visible until every payload check
  // for this packet has passed.
  XenosState next_state = state;
  std::vector<XenosCommand> staged;
  staged.reserve(1u);

  auto require_words = [&](std::size_t required) -> Pm4Error {
    if (words.size() < required) {
      return make_error(Pm4ErrorCode::kTruncatedPacket, words.size(),
                        "PM4 packet payload is truncated");
    }
    return {};
  };

  switch (type) {
    case pm4::kType0: {
      const Pm4Error length_error = require_words(1u + count);
      if (length_error) return {0u, length_error};
      const std::uint32_t base = low_register_of(header);
      if (base >= XenosState::kRegisterCount ||
          (!one_reg_of(header) && count > XenosState::kRegisterCount - base)) {
        return {0u, make_error(Pm4ErrorCode::kInvalidRegister, 0u,
                               "TYPE0 register range exceeds Xenos state")};
      }
      for (std::uint32_t index = 0u; index < count; ++index) {
        const std::uint32_t reg = one_reg_of(header) ? base : base + index;
        if (!next_state.set_register(reg, words[1u + index])) {
          return {0u, make_error(Pm4ErrorCode::kInvalidRegister, 1u + index,
                                 "TYPE0 register is not writable")};
        }
      }
      state = next_state;
      return {1u + count, {}};
    }
    case pm4::kType1: {
      // Hardware TYPE1 always contains exactly two values.  The two register
      // indices live in the packet header (11 bits each).
      const Pm4Error length_error = require_words(3u);
      if (length_error) return {0u, length_error};
      if (count != 2u) {
        return {0u, make_error(Pm4ErrorCode::kInvalidCount, 0u,
                               "TYPE1 packet must contain two payload values")};
      }
      const std::array<std::uint32_t, 2u> registers = {
          type1_reg_a(header), type1_reg_b(header)};
      for (std::uint32_t index = 0u; index < 2u; ++index) {
        const std::uint32_t reg = registers[index];
        if (reg >= XenosState::kRegisterCount ||
            !next_state.set_register(reg, words[1u + index])) {
          return {0u, make_error(Pm4ErrorCode::kInvalidRegister, 1u + index,
                                 "TYPE1 register is outside Xenos state")};
        }
      }
      state = next_state;
      return {1u + count, {}};
    }
    case pm4::kType2:
      if ((header & 0x3FFFFFFFu) != 0u) {
        return {0u, make_error(Pm4ErrorCode::kInvalidPayload, 0u,
                               "TYPE2 NOP has non-zero payload bits")};
      }
      return {1u, {}};
    case pm4::kType3:
      break;
    default:
      return {0u, make_error(Pm4ErrorCode::kInvalidType, 0u,
                             "unknown PM4 packet type")};
  }

  const Pm4Error length_error = require_words(1u + count);
  if (length_error) return {0u, length_error};
  const std::uint32_t opcode = opcode_of(header);
  if ((header & 1u) != 0u) {
    return {0u, make_error(Pm4ErrorCode::kInvalidPayload, 0u,
                           "predicated TYPE3 packets are not supported")};
  }
  const std::span<const std::uint32_t> payload(words.data() + 1u, count);
  switch (opcode) {
    case pm4::kOpcodeNop:
      break;
    case pm4::kOpcodeSetBinMaskLo:
    case pm4::kOpcodeSetBinMaskHi:
    case pm4::kOpcodeSetBinSelectLo:
    case pm4::kOpcodeSetBinSelectHi:
      if (count != 1u) {
        return {0u, make_error(Pm4ErrorCode::kInvalidPayload, 1u,
                               "binning state packet requires one payload dword")};
      }
      break;
    case pm4::kOpcodeSetBinMask:
    case pm4::kOpcodeSetBinSelect:
      if (count != 2u) {
        return {0u, make_error(Pm4ErrorCode::kInvalidPayload, 1u,
                               "binning state packet requires two payload dwords")};
      }
      break;
    case pm4::kOpcodeImLoadImmediate: {
      if (count < 2u || (payload[1] >> 16u) != 0u ||
          (payload[1] & 0xffffu) > count - 2u ||
          (payload[0] > 1u)) {
        return {0u, make_error(Pm4ErrorCode::kInvalidPayload, 1u,
                               "IM_LOAD_IMMEDIATE shader envelope is invalid")};
      }
      staged.emplace_back(ImmediateShaderPacket{payload[0], payload[1] >> 16u,
                                                payload[1] & 0xffffu});
      break;
    }
    case pm4::kOpcodeRegRmw: {
      if (count != 3u) {
        return {0u, make_error(Pm4ErrorCode::kInvalidPayload, 1u,
                               "REG_RMW requires three payload dwords")};
      }
      const std::uint32_t rmw_info = payload[0];
      const std::uint32_t target = rmw_info & 0x1FFFu;
      const std::uint32_t and_index = payload[1] & 0x1FFFu;
      const std::uint32_t or_index = payload[2] & 0x1FFFu;
      std::uint32_t value = next_state.register_value(target);
      value &= (rmw_info >> 31u) != 0u
                   ? next_state.register_value(and_index)
                   : payload[1];
      value |= (rmw_info >> 30u) != 0u
                   ? next_state.register_value(or_index)
                   : payload[2];
      if (!next_state.set_register(target, value)) {
        return {0u, make_error(Pm4ErrorCode::kInvalidRegister, 1u,
                               "REG_RMW target register is not writable")};
      }
      break;
    }
    case pm4::kOpcodeInvalidateState:
      if (count != 1u) {
        return {0u, make_error(Pm4ErrorCode::kInvalidPayload, 1u,
                               "INVALIDATE_STATE requires one payload dword")};
      }
      // The payload is a driver state-pointer mask. The immutable native
      // state snapshot is already replaced transactionally below.
      break;
    case pm4::kOpcodeEventWriteShd:
      if (count != 3u || (payload[0] & ~0x8000003Fu) != 0u) {
        return {0u, make_error(Pm4ErrorCode::kInvalidPayload, 1u,
                               "EVENT_WRITE_SHD envelope is invalid")};
      }
      staged.emplace_back(EventWriteShdPacket{payload[0], payload[1], payload[2]});
      break;
    case pm4::kOpcodeMeInit:
      if (count != 18u) {
        return {0u, make_error(Pm4ErrorCode::kInvalidPayload, 1u,
                               "PM4_ME_INIT requires 18 payload dwords")};
      }
      {
        MicroEngineInitPacket packet{};
        std::copy(payload.begin(), payload.end(), packet.payload.begin());
        staged.emplace_back(packet);
      }
      break;
    case pm4::kOpcodeWaitForIdle:
      if (count != 1u || payload[0] != 0u) {
        return {0u, make_error(Pm4ErrorCode::kInvalidWait, 1u,
                               "WAIT_FOR_IDLE requires a zero payload")};
      }
      staged.emplace_back(WaitPacket{0u});
      break;
    case pm4::kOpcodeWaitRegMem:
      if (count != 5u || (payload[0] & ~0x17u) != 0u ||
          (payload[0] & 7u) > 7u) {
        return {0u, make_error(Pm4ErrorCode::kInvalidWait, 1u,
                               "WAIT_REG_MEM envelope is invalid")};
      }
      staged.emplace_back(WaitPacket{payload[0] & 7u});
      break;
    case pm4::kOpcodeDrawIndx:
      if (count < 2u || payload[1] == 0u ||
          !valid_primitive(payload[1] & 0x3Fu) ||
          ((payload[1] >> 6u) & 3u) == 1u ||
          ((payload[1] >> 6u) & 3u) == 3u) {
        return {0u, make_error(Pm4ErrorCode::kInvalidPayload, 1u,
                               "DRAW_INDX payload or source selector is invalid")};
      }
      {
        const std::uint32_t index_count = (payload[1] >> 16u) & 0xFFFFu;
        const std::uint32_t source = (payload[1] >> 6u) & 3u;
        const std::uint32_t index_format =
            source == 0u ? ((payload[3] >> 11u) & 1u) : 0u;
        if (index_count == 0u ||
            (source == 0u && (count < 4u || payload[2] == 0u))) {
          return {0u, make_error(Pm4ErrorCode::kInvalidPayload, 2u,
                                 "DRAW_INDX index range is empty")};
        }
        staged.emplace_back(DrawPacket{payload[1] & 0x3Fu, index_count,
                                       index_count, 0u,
                                       source == 0u ? payload[2] : 0u,
                                       std::max(1u, payload[3] & 0x7FFu),
                                       index_format, next_state.generation()});
      }
      break;
    case pm4::kOpcodeDrawIndx2:
      if (count < 1u || payload[0] == 0u ||
          !valid_primitive(payload[0] & 0x3Fu) ||
          ((payload[0] >> 6u) & 3u) != 2u) {
        return {0u, make_error(Pm4ErrorCode::kInvalidPayload, 1u,
                               "DRAW_INDX_2 requires auto-indexed source")};
      }
      staged.emplace_back(DrawPacket{payload[0] & 0x3Fu,
                                     (payload[0] >> 16u) & 0xFFFFu,
                                     (payload[0] >> 16u) & 0xFFFFu, 0u, 0u,
                                     1u, 0u, next_state.generation()});
      break;
    case pm4::kOpcodeSetConstant: {
      if (count < 2u || ((payload[0] >> 16u) & 0xFFu) != 4u) {
        return {0u, make_error(Pm4ErrorCode::kInvalidPayload, 1u,
                               "SET_CONSTANT only accepts direct registers")};
      }
      const std::uint32_t base = payload[0] & 0x7FFu;
      const std::uint32_t values = count - 1u;
      if (base >= XenosState::kRegisterCount ||
          values > XenosState::kRegisterCount - base) {
        return {0u, make_error(Pm4ErrorCode::kInvalidRegister, 1u,
                               "SET_CONSTANT register range exceeds state")};
      }
      for (std::uint32_t i = 0u; i < values; ++i) {
        if (!next_state.set_register(base + i, payload[1u + i])) {
          return {0u, make_error(Pm4ErrorCode::kInvalidRegister, 1u + i,
                                 "SET_CONSTANT register is not writable")};
        }
      }
      break;
    }
    case pm4::kOpcodeContextUpdate:
      if (count != 1u || payload[0] != 0u) {
        return {0u, make_error(Pm4ErrorCode::kInvalidPayload, 1u,
                               "CONTEXT_UPDATE payload is invalid")};
      }
      break;
    case pm4::kOpcodeInterrupt:
      if (count != 1u || payload[0] > 0x3Fu) {
        return {0u, make_error(Pm4ErrorCode::kInvalidPayload, 1u,
                               "INTERRUPT payload is invalid")};
      }
      break;
    case pm4::kOpcodeXeSwap:
      if (count < 4u || payload[0] != pm4::kSwapSignature ||
          !valid_dimensions(payload[2], payload[3])) {
        return {0u, make_error(Pm4ErrorCode::kInvalidPayload, 1u,
                               "XE_SWAP signature or dimensions are invalid")};
      }
      // XE_SWAP carries a front-buffer guest pointer, not a small render-target
      // index.  The native present boundary resolves that pointer to surface 0
      // after its bounded range has been validated.
      staged.emplace_back(PresentPacket{0u, payload[2], payload[3], 0u});
      break;
    case pm4::kOpcodeIndirectBuffer:
      if (count != 2u || payload[0] == 0u || payload[1] == 0u ||
          payload[1] > (1u << 20u)) {
        return {0u, make_error(Pm4ErrorCode::kInvalidPayload, 1u,
                               "INDIRECT_BUFFER range is invalid")};
      }
      staged.emplace_back(IndirectBufferPacket{payload[0], payload[1]});
      break;
    default:
      return {0u, make_error(Pm4ErrorCode::kUnsupportedOpcode, 0u,
                             "unsupported TYPE3 opcode")};
  }

  output.insert(output.end(), staged.begin(), staged.end());
  state = next_state;
  return {1u + count, {}};
}

DecodeResult Pm4Decoder::decode_stream(std::span<const std::uint32_t> words,
                                       XenosState& state,
                                       std::vector<XenosCommand>& output) {
  std::size_t offset = 0u;
  while (offset < words.size()) {
    const DecodeResult result = decode_one(words.subspan(offset), state, output);
    if (!result.ok()) {
      Pm4Error error = result.error;
      error.dword_offset += offset;
      return {offset, std::move(error)};
    }
    if (result.consumed == 0u || result.consumed > words.size() - offset) {
      return {offset, make_error(Pm4ErrorCode::kRingCorrupt, offset,
                                 "decoder made no forward progress")};
    }
    offset += result.consumed;
  }
  return {offset, {}};
}

MmioBus::MmioBus(InterruptCallback callback) : callback_(std::move(callback)) {}

bool MmioBus::fail(Pm4ErrorCode code, const char* detail) {
  error_ = Pm4Error{code, 0u, detail};
  return false;
}

bool MmioBus::write(GuestAddress address, std::uint32_t value) {
  error_ = {};
  switch (address) {
    case kRingBase:
      if ((value & 0xFFFu) != 0u) return fail(Pm4ErrorCode::kInvalidRange,
                                               "ring base is not 4 KiB aligned");
      ring_base_ = value;
      return true;
    case kRingSize:
      if (value < 256u || value > (1u << 20u) || (value & (value - 1u)) != 0u) {
        return fail(Pm4ErrorCode::kInvalidRange,
                    "ring size must be a power of two between 256 and 1 MiB");
      }
      ring_size_ = value;
      ring_read_ = ring_write_ = 0u;
      return true;
    case kRingRead:
      if (ring_size_ == 0u || (value & 3u) != 0u || value >= ring_size_) {
        return fail(Pm4ErrorCode::kInvalidRange, "ring read pointer is out of range");
      }
      ring_read_ = value;
      return true;
    case kRingWrite:
      if (ring_size_ == 0u || (value & 3u) != 0u || value >= ring_size_) {
        return fail(Pm4ErrorCode::kInvalidRange, "ring write pointer is out of range");
      }
      ring_write_ = value;
      return true;
    case kInterrupt:
      if (callback_) callback_(value);
      return true;
    default:
      return fail(Pm4ErrorCode::kInvalidPayload, "unknown Xenos MMIO register");
  }
}

bool MmioBus::read(GuestAddress address, std::uint32_t& value) const noexcept {
  switch (address) {
    case kRingBase: value = ring_base_; return true;
    case kRingSize: value = ring_size_; return true;
    case kRingRead: value = ring_read_; return true;
    case kRingWrite: value = ring_write_; return true;
    default: return false;
  }
}

DecodeResult VdBridge::pump(XenosState& state,
                            std::vector<XenosCommand>& output) {
  error_ = {};
  if (bus_.ring_size() == 0u || (bus_.ring_size() % 4u) != 0u ||
      ring_words_.size() != bus_.ring_size() / 4u) {
    error_ = make_error(Pm4ErrorCode::kRingCorrupt, 0u,
                        "ring memory does not match MMIO ring size");
    return {0u, error_};
  }
  const std::size_t word_count = ring_words_.size();
  const std::size_t read = bus_.ring_read() / 4u;
  const std::size_t write = bus_.ring_write() / 4u;
  if ((bus_.ring_read() & 3u) != 0u || (bus_.ring_write() & 3u) != 0u ||
      read >= word_count || write >= word_count) {
    error_ = make_error(Pm4ErrorCode::kRingCorrupt, 0u,
                        "ring pointer is not dword aligned");
    return {0u, error_};
  }
  if (read == write) return {0u, {}};
  const std::size_t available = write > read ? write - read
                                             : word_count - read + write;
  std::vector<std::uint32_t> packet;
  packet.reserve(available);
  for (std::size_t index = 0u; index < available; ++index) {
    packet.push_back(ring_words_[(read + index) % word_count]);
  }
  XenosState candidate = state;
  std::vector<XenosCommand> decoded;
  const DecodeResult result = Pm4Decoder::decode_stream(packet, candidate, decoded);
  if (!result.ok()) {
    error_ = result.error;
    return result;
  }
  std::vector<XenosCommand> expanded;
  std::vector<GuestAddress> active_ib;
  const auto expand_indirect = [&](const auto& self,
                                   std::span<const XenosCommand> commands,
                                   std::size_t depth) -> bool {
    if (depth > 8u) {
      error_ = make_error(Pm4ErrorCode::kRingCorrupt, 0u,
                          "INDIRECT_BUFFER nesting exceeds bound");
      return false;
    }
    for (const XenosCommand& command : commands) {
      if (const auto* indirect = std::get_if<IndirectBufferPacket>(&command)) {
        if (std::find(active_ib.begin(), active_ib.end(), indirect->address) !=
            active_ib.end()) {
          error_ = make_error(Pm4ErrorCode::kRingCorrupt, 0u,
                              "INDIRECT_BUFFER is cyclic");
          return false;
        }
        std::vector<std::uint32_t> nested_words;
        std::span<const std::uint32_t> nested_span;
        if (guest_memory_ != nullptr) {
          const std::uint64_t byte_count =
              static_cast<std::uint64_t>(indirect->dword_count) * 4u;
          if (static_cast<std::uint64_t>(indirect->address) + byte_count >
              (1ull << 32u)) {
            error_ = make_error(Pm4ErrorCode::kRingCorrupt, 0u,
                                "INDIRECT_BUFFER range exceeds guest memory");
            return false;
          }
          nested_words.resize(indirect->dword_count);
          for (std::size_t index = 0u; index < nested_words.size(); ++index) {
            std::uint32_t encoded = 0u;
            std::memcpy(&encoded,
                        guest_memory_ + indirect->address + index * 4u,
                        sizeof(encoded));
            nested_words[index] = __builtin_bswap32(encoded);
          }
          nested_span = std::span<const std::uint32_t>(nested_words);
        } else {
          if (guest_words_.empty() || indirect->address < guest_base_ ||
              (indirect->address - guest_base_) % 4u != 0u) {
            error_ = make_error(Pm4ErrorCode::kRingCorrupt, 0u,
                                "INDIRECT_BUFFER address is missing");
            return false;
          }
          const std::size_t guest_offset =
              static_cast<std::size_t>((indirect->address - guest_base_) / 4u);
          if (guest_offset > guest_words_.size() ||
              indirect->dword_count > guest_words_.size() - guest_offset) {
            error_ = make_error(Pm4ErrorCode::kRingCorrupt, 0u,
                                "INDIRECT_BUFFER range exceeds guest memory");
            return false;
          }
          nested_span = std::span<const std::uint32_t>(
              guest_words_.data() + guest_offset, indirect->dword_count);
        }
        active_ib.push_back(indirect->address);
        std::vector<XenosCommand> nested;
        const DecodeResult nested_result = Pm4Decoder::decode_stream(
            nested_span, candidate, nested);
        if (!nested_result.ok()) {
          error_ = nested_result.error;
          error_.detail += " (IB ";
          error_.detail += to_hex(indirect->address);
          if (nested_result.error.dword_offset < nested_span.size()) {
            error_.detail += ", header ";
            error_.detail += to_hex(nested_span[nested_result.error.dword_offset]);
          }
          error_.detail += ")";
          return false;
        }
        if (!self(self, std::span<const XenosCommand>(nested), depth + 1u)) {
          return false;
        }
        active_ib.pop_back();
      } else {
        expanded.push_back(command);
      }
    }
    return true;
  };
  if (!expand_indirect(expand_indirect, std::span<const XenosCommand>(decoded), 0u)) {
    return {0u, error_};
  }
  const std::size_t consumed_words = result.consumed;
  const std::uint32_t new_read = static_cast<std::uint32_t>(
      ((read + consumed_words) % word_count) * 4u);
  if (!bus_.write(MmioBus::kRingRead, new_read)) {
    error_ = bus_.error();
    return {0u, error_};
  }
  state = candidate;
  output.insert(output.end(), expanded.begin(), expanded.end());
  return {consumed_words, {}};
}

}  // namespace ac6::native

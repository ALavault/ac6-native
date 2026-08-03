/**
 * Targeted AC6 guest-text argument probe.
 *
 * The renderer and font atlas are already proven to draw PLEASE WAIT, YES/NO,
 * and the font inventory. This instrument instead records which generated UI
 * function receives each printable guest string, including one level of
 * pointer indirection for the game's string objects. It only reads ranges the
 * guest memory manager reports as committed and readable; unlike the retired
 * /proc/self/maps scanner, it never walks file-backed holes that can SIGBUS.
 */

#include "ac6_guest_text_probe.h"

#ifdef AC6RECOMP_PROBE_GUEST_TEXT

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstdint>
#include <limits>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_set>

#include <native/memory/utils.h>
#include <rex/cvar.h>
#include <rex/logging.h>
#include <rex/ppc/context.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xmemory.h>

REXCVAR_DEFINE_BOOL(ac6_probe_guest_text, false, "AC6/Text",
                    "Capture printable string arguments in the generated AC6 UI functions.");
REXCVAR_DEFINE_INT32(ac6_probe_guest_text_entries, 200000, "AC6/Text",
                     "Maximum generated UI function entries examined by the text probe.");
REXCVAR_DEFINE_INT32(ac6_probe_guest_text_lines, 512, "AC6/Text",
                     "Maximum unique strings emitted by the text probe.");

namespace ac6::text_probe {
namespace {

constexpr size_t kPreviewBytes = 128;
constexpr size_t kNestedWords = 16;

std::atomic<bool> g_started{false};
std::atomic<bool> g_armed{false};
std::atomic<uint32_t> g_entries{0};
std::mutex g_mutex;
std::unordered_set<uint64_t> g_seen;
uint32_t g_lines = 0;

struct DecodedText {
  const char* encoding = nullptr;
  std::string preview;
};

bool IsReadable(rex::memory::Memory* memory, uint32_t address, size_t size) {
  if (!memory || address == 0 || size == 0 ||
      size - 1 > std::numeric_limits<uint32_t>::max() - address) {
    return false;
  }
  const uint32_t last = address + static_cast<uint32_t>(size - 1);
  auto* heap = memory->LookupHeap(address);
  if (!heap || memory->LookupHeap(last) != heap) {
    return false;
  }
  return heap->QueryRangeAccess(address, last) != rex::memory::PageAccess::kNoAccess;
}

bool IsTextByte(uint8_t value) {
  return (value >= 0x20 && value <= 0x7E) || value == '\n' || value == '\r' ||
         value == '\t';
}

void AppendEscaped(std::string& out, uint8_t value) {
  switch (value) {
    case '\\': out += "\\\\"; break;
    case '"': out += "\\\""; break;
    case '\n': out += "\\n"; break;
    case '\r': out += "\\r"; break;
    case '\t': out += "\\t"; break;
    default: out.push_back(static_cast<char>(value)); break;
  }
}

bool HasAlnum(std::string_view text) {
  for (unsigned char c : text) {
    if (std::isalnum(c)) return true;
  }
  return false;
}

DecodedText DecodeAscii(const uint8_t* bytes) {
  DecodedText result;
  result.encoding = "ascii";
  for (size_t i = 0; i < kPreviewBytes; ++i) {
    const uint8_t value = bytes[i];
    if (value == 0) {
      if (result.preview.size() >= 2 && HasAlnum(result.preview)) return result;
      return {};
    }
    if (!IsTextByte(value)) return {};
    AppendEscaped(result.preview, value);
  }
  return {};
}

DecodedText DecodeUtf16(const uint8_t* bytes, bool big_endian) {
  DecodedText result;
  result.encoding = big_endian ? "utf16be" : "utf16le";
  for (size_t i = 0; i < kPreviewBytes; i += 2) {
    const uint8_t high = bytes[i + (big_endian ? 0 : 1)];
    const uint8_t low = bytes[i + (big_endian ? 1 : 0)];
    if (high == 0 && low == 0) {
      if (result.preview.size() >= 2 && HasAlnum(result.preview)) return result;
      return {};
    }
    if (high != 0 || !IsTextByte(low)) return {};
    AppendEscaped(result.preview, low);
  }
  return {};
}

DecodedText DecodeAt(rex::memory::Memory* memory, uint32_t address) {
  if (!IsReadable(memory, address, kPreviewBytes)) return {};
  const uint8_t* bytes = memory->TranslateVirtual<const uint8_t*>(address);
  if (DecodedText text = DecodeAscii(bytes); text.encoding) return text;
  if (DecodedText text = DecodeUtf16(bytes, true); text.encoding) return text;
  return DecodeUtf16(bytes, false);
}

uint32_t LoadBe32(const uint8_t* bytes) {
  return (uint32_t(bytes[0]) << 24) | (uint32_t(bytes[1]) << 16) |
         (uint32_t(bytes[2]) << 8) | uint32_t(bytes[3]);
}

uint64_t SeenKey(const char* function_name, uint32_t guest_lr, uint32_t pointer) {
  uint64_t value = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(function_name));
  value ^= uint64_t(pointer) * 0x9E3779B185EBCA87ull;
  value ^= uint64_t(guest_lr) * 0xC2B2AE3D27D4EB4Full;
  value ^= value >> 29;
  return value;
}

void Emit(const char* function_name, uint32_t guest_lr, const char* reg_name, uint32_t owner,
          uint32_t pointer, int nested_word, const DecodedText& text) {
  const int32_t max_lines = REXCVAR_GET(ac6_probe_guest_text_lines);
  if (max_lines <= 0 || g_lines >= static_cast<uint32_t>(max_lines)) return;
  ++g_lines;
  if (nested_word < 0) {
    REXLOG_WARN("[ac6-text] fn={} lr=0x{:08X} reg={} ptr=0x{:08X} depth=0 encoding={} "
                "text=\"{}\"",
                function_name, guest_lr, reg_name, pointer, text.encoding, text.preview);
  } else {
    REXLOG_WARN("[ac6-text] fn={} lr=0x{:08X} reg={} owner=0x{:08X} word={} "
                "ptr=0x{:08X} depth=1 encoding={} text=\"{}\"",
                function_name, guest_lr, reg_name, owner, nested_word, pointer,
                text.encoding, text.preview);
  }
}

void InspectPointer(rex::memory::Memory* memory, const char* function_name, uint32_t guest_lr,
                    const char* reg_name, uint32_t pointer) {
  if (pointer < 0x10000 ||
      !g_seen.insert(SeenKey(function_name, guest_lr, pointer)).second) return;

  if (DecodedText text = DecodeAt(memory, pointer); text.encoding) {
    Emit(function_name, guest_lr, reg_name, pointer, pointer, -1, text);
  }

  constexpr size_t kNestedBytes = kNestedWords * sizeof(uint32_t);
  if (!IsReadable(memory, pointer, kNestedBytes)) return;
  const uint8_t* object = memory->TranslateVirtual<const uint8_t*>(pointer);
  for (size_t word = 0; word < kNestedWords; ++word) {
    const uint32_t nested = LoadBe32(object + word * sizeof(uint32_t));
    if (nested < 0x10000 || nested == pointer) continue;
    if (DecodedText text = DecodeAt(memory, nested); text.encoding) {
      Emit(function_name, guest_lr, reg_name, pointer, nested, static_cast<int>(word), text);
    }
  }
}

}  // namespace

void Arm() {
  if (!REXCVAR_GET(ac6_probe_guest_text) || g_started.exchange(true)) return;

  g_entries.store(0, std::memory_order_relaxed);
  g_armed.store(true, std::memory_order_release);
  REXLOG_WARN("[ac6-text] armed after dialog screen update; probing six generated UI units");
}

void Note(const char* function_name, PPCContext& ctx, uint8_t* base) {
  if (!g_armed.load(std::memory_order_relaxed)) return;

  int32_t configured_limit = REXCVAR_GET(ac6_probe_guest_text_entries);
  if (configured_limit <= 0) configured_limit = 1;
  const uint32_t entry = g_entries.fetch_add(1, std::memory_order_relaxed);
  if (entry >= static_cast<uint32_t>(configured_limit)) {
    if (g_armed.exchange(false, std::memory_order_acq_rel)) {
      REXLOG_WARN("[ac6-text] capture complete: {} entries, {} unique string records",
                  entry, g_lines);
    }
    return;
  }

  std::unique_lock lock(g_mutex, std::try_to_lock);
  if (!lock.owns_lock() || !ctx.kernel_state) return;
  auto* memory = ctx.kernel_state->memory();
  if (!memory || memory->virtual_membase() != base) return;

  const std::array<std::pair<const char*, uint32_t>, 9> registers = {{
      {"r3", ctx.r3.u32}, {"r4", ctx.r4.u32}, {"r5", ctx.r5.u32},
      {"r6", ctx.r6.u32}, {"r7", ctx.r7.u32}, {"r8", ctx.r8.u32},
      {"r9", ctx.r9.u32}, {"r10", ctx.r10.u32}, {"r11", ctx.r11.u32},
  }};
  const uint32_t guest_lr = static_cast<uint32_t>(ctx.lr);
  for (const auto& [reg_name, pointer] : registers) {
    InspectPointer(memory, function_name, guest_lr, reg_name, pointer);
  }
}

void CallIndirect(uint32_t target, PPCFunc* function, PPCContext& ctx, uint8_t* base) {
  constexpr uint32_t kTextPrepareReturn = 0x8237D750u;
  constexpr uint32_t kTextLookupReturn = 0x820F885Cu;
  constexpr uint32_t kTextMetricsReturn = 0x820F8924u;
  constexpr uint32_t kTextDrawReturn = 0x820F8990u;
  constexpr uint32_t kTextBufferReturn = 0x8228CA14u;
  constexpr uint32_t kTextMetadataReturn = 0x8228CA34u;
  constexpr uint32_t kTextSubmitReturn = 0x8228CA6Cu;
  constexpr uint32_t kTextBufferRecordReturn = 0x822EFEFCu;
  constexpr uint32_t kTextLengthRecordReturn = 0x822EFF44u;
  static std::atomic<uint32_t> prepare_lines{0};
  static std::atomic<uint32_t> lookup_lines{0};
  static std::atomic<uint32_t> stage_lines{0};
  static thread_local char last_lookup_key[65] = {};
  static thread_local int32_t last_lookup_id = -1;
  const uint32_t return_address = static_cast<uint32_t>(ctx.lr);
  const bool inspect_prepare = g_started.load(std::memory_order_relaxed) &&
                               return_address == kTextPrepareReturn &&
                               prepare_lines.fetch_add(1, std::memory_order_relaxed) < 256;
  const bool inspect_lookup = g_started.load(std::memory_order_relaxed) &&
                              return_address == kTextLookupReturn &&
                              lookup_lines.fetch_add(1, std::memory_order_relaxed) < 256;
  const bool inspect_stage = g_started.load(std::memory_order_relaxed) &&
                             (return_address == kTextMetricsReturn ||
                              return_address == kTextDrawReturn ||
                              return_address == kTextBufferReturn ||
                              return_address == kTextMetadataReturn ||
                              return_address == kTextSubmitReturn) &&
                             stage_lines.fetch_add(1, std::memory_order_relaxed) < 1024;
  const bool inspect_record = g_started.load(std::memory_order_relaxed) &&
                              (return_address == kTextBufferRecordReturn ||
                               return_address == kTextLengthRecordReturn) &&
                              (std::string_view(last_lookup_key) == "M70000_122" ||
                               std::string_view(last_lookup_key) == "M70000_222");
  const uint32_t out = ctx.r6.u32;
  const uint32_t aux = ctx.r7.u32;
  const uint32_t before_r3 = ctx.r3.u32;
  const uint32_t before_r4 = ctx.r4.u32;
  const uint32_t before_r5 = ctx.r5.u32;
  const uint32_t before_r6 = ctx.r6.u32;
  const double before_f1 = ctx.f1.f64;
  char lookup_key[65] = {};
  if (inspect_lookup) {
    const uint32_t key = ctx.r4.u32;
    for (size_t i = 0; key && i + 1 < sizeof(lookup_key); ++i) {
      const uint8_t value = base[key + i];
      if (value == 0 || value < 0x20 || value > 0x7E) break;
      lookup_key[i] = static_cast<char>(value);
    }
    REXLOG_WARN("[ac6-text-lookup] before target=0x{:08X} resolver=0x{:08X} "
                "key_ptr=0x{:08X} key=\"{}\"",
                target, ctx.r3.u32, key, lookup_key);
  }
  if (inspect_prepare) {
    REXLOG_WARN("[ac6-text-prepare] before target=0x{:08X} r3=0x{:08X} r4=0x{:08X} "
                "r5=0x{:08X} out=0x{:08X} aux=0x{:08X}",
                target, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, out, aux);
  }
  function(ctx, base);
  if (inspect_record && ctx.r3.u32) {
    const uint32_t record = ctx.r3.u32;
    REXLOG_WARN("[ac6-text-record] return=0x{:08X} target=0x{:08X} key=\"{}\" "
                "lookup_id={} ptr=0x{:08X} words=[{:08X} {:08X} {:08X} {:08X}] "
                "next=[{:08X} {:08X} {:08X} {:08X}]",
                return_address, target, last_lookup_key, last_lookup_id, record,
                rex::memory::load_and_swap<uint32_t>(base + record + 0),
                rex::memory::load_and_swap<uint32_t>(base + record + 4),
                rex::memory::load_and_swap<uint32_t>(base + record + 8),
                rex::memory::load_and_swap<uint32_t>(base + record + 12),
                rex::memory::load_and_swap<uint32_t>(base + record + 32),
                rex::memory::load_and_swap<uint32_t>(base + record + 36),
                rex::memory::load_and_swap<uint32_t>(base + record + 40),
                rex::memory::load_and_swap<uint32_t>(base + record + 44));
  }
  if (inspect_stage && return_address == kTextBufferReturn && ctx.r3.u32) {
    const uint32_t pointer = ctx.r3.u32;
    REXLOG_WARN("[ac6-text-buffer] key=\"{}\" lookup_id={} ptr=0x{:08X} "
                "words=[{:08X} {:08X} {:08X} {:08X} {:08X} {:08X} {:08X} {:08X}]",
                last_lookup_key, last_lookup_id, pointer,
                rex::memory::load_and_swap<uint32_t>(base + pointer + 0),
                rex::memory::load_and_swap<uint32_t>(base + pointer + 4),
                rex::memory::load_and_swap<uint32_t>(base + pointer + 8),
                rex::memory::load_and_swap<uint32_t>(base + pointer + 12),
                rex::memory::load_and_swap<uint32_t>(base + pointer + 16),
                rex::memory::load_and_swap<uint32_t>(base + pointer + 20),
                rex::memory::load_and_swap<uint32_t>(base + pointer + 24),
                rex::memory::load_and_swap<uint32_t>(base + pointer + 28));
  }
  if (inspect_lookup) {
    std::copy(std::begin(lookup_key), std::end(lookup_key), std::begin(last_lookup_key));
    last_lookup_id = ctx.r3.s32;
    REXLOG_WARN("[ac6-text-lookup] after  target=0x{:08X} result={} (0x{:08X}) key=\"{}\"",
                target, ctx.r3.s32, ctx.r3.u32, lookup_key);
  }
  if (inspect_stage) {
    REXLOG_WARN("[ac6-text-stage] return=0x{:08X} target=0x{:08X} key=\"{}\" "
                "lookup_id={} before=[r3=0x{:08X} r4={} r5=0x{:08X} r6=0x{:08X} f1={}] "
                "after=[r3=0x{:08X} f1={}]",
                return_address, target, last_lookup_key, last_lookup_id, before_r3,
                static_cast<int32_t>(before_r4), before_r5, before_r6, before_f1,
                ctx.r3.u32, ctx.f1.f64);
  }
  if (inspect_prepare) {
    const uint32_t out0 = out ? rex::memory::load_and_swap<uint32_t>(base + out) : 0;
    const uint32_t out1 = out ? rex::memory::load_and_swap<uint32_t>(base + out + 4) : 0;
    const uint32_t aux0 = aux ? rex::memory::load_and_swap<uint32_t>(base + aux) : 0;
    char text[65] = {};
    if (out) {
      for (size_t i = 0; i + 1 < sizeof(text); ++i) {
        const uint8_t value = base[out + i];
        if (value == 0 || value < 0x20 || value > 0x7E) break;
        text[i] = static_cast<char>(value);
      }
    }
    REXLOG_WARN("[ac6-text-prepare] after  target=0x{:08X} r3=0x{:08X} "
                "out[0..1]=0x{:08X},0x{:08X} aux[0]=0x{:08X} text=\"{}\"",
                target, ctx.r3.u32, out0, out1, aux0, text);
  }
}

}  // namespace ac6::text_probe

#endif  // AC6RECOMP_PROBE_GUEST_TEXT

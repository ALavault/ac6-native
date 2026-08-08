#include "ac6/retail_ndxr_container.h"

#include <cstring>

namespace ac6::retail {
namespace {

std::uint16_t Be16(const std::uint8_t* p) noexcept {
  return static_cast<std::uint16_t>((static_cast<std::uint16_t>(p[0]) << 8) | p[1]);
}

std::uint32_t Be32(const std::uint8_t* p) noexcept {
  return (static_cast<std::uint32_t>(p[0]) << 24) | (static_cast<std::uint32_t>(p[1]) << 16) |
         (static_cast<std::uint32_t>(p[2]) << 8) | static_cast<std::uint32_t>(p[3]);
}

// 0x8233EF48 and 0x8233EF68 compare the first dword against a literal. The
// retail form is branchless; the comparison is what matters, not the trick.
bool MagicIs(const std::uint8_t* p, std::uint32_t literal) noexcept {
  return Be32(p) == literal;
}

constexpr std::uint32_t kNdxrMagic = 0x4E445852u;  // "NDXR", 0x8233EF48
constexpr std::uint32_t kGidxMagic = 0x47494458u;  // "GIDX", 0x8233EF68

void SetRefusal(NdxrRefusal* out, NdxrRefusal value) noexcept {
  if (out != nullptr) *out = value;
}

}  // namespace

const char* RefusalToString(NdxrRefusal refusal) noexcept {
  switch (refusal) {
    case NdxrRefusal::kNone: return "none";
    case NdxrRefusal::kTooSmall: return "too-small";
    case NdxrRefusal::kBadMagic: return "bad-magic";
    case NdxrRefusal::kSizeMismatch: return "size-mismatch";
    case NdxrRefusal::kUnsupportedCode: return "unsupported-code";
    case NdxrRefusal::kNoRecords: return "no-records";
    case NdxrRefusal::kSectionOverflow: return "section-overflow";
    case NdxrRefusal::kRecordOverflow: return "record-overflow";
    case NdxrRefusal::kStringOverflow: return "string-overflow";
  }
  return "unknown";
}

std::optional<NdxrContainer> NdxrContainer::Open(
    const std::uint8_t* bytes, std::size_t size,
    NdxrRefusal* refusal) noexcept {
  SetRefusal(refusal, NdxrRefusal::kNone);
  if (bytes == nullptr || size < kRecordArrayBase) {
    SetRefusal(refusal, NdxrRefusal::kTooSmall);
    return std::nullopt;
  }

  NdxrContainer container;
  const std::uint8_t* base = bytes;
  std::size_t remaining = size;

  // 0x8234CA28: the GIDX arm advances 0x10 and re-tests for NDXR, and does
  // nothing else. So a GIDX file is a header in front of an NDXR.
  if (MagicIs(base, kGidxMagic)) {
    if (remaining < kGidxHeaderSize + kRecordArrayBase) {
      SetRefusal(refusal, NdxrRefusal::kTooSmall);
      return std::nullopt;
    }
    base += kGidxHeaderSize;
    remaining -= kGidxHeaderSize;
    container.gidx_wrapped_ = true;
  }

  if (!MagicIs(base, kNdxrMagic)) {
    SetRefusal(refusal, NdxrRefusal::kBadMagic);
    return std::nullopt;
  }

  // [+0x04] is the file's own byte length. Retail does not check this; it is
  // free here and it is the cheapest possible guard on a truncated read.
  if (Be32(base + 0x04) != remaining) {
    SetRefusal(refusal, NdxrRefusal::kSizeMismatch);
    return std::nullopt;
  }

  // 0x8234CA28 returns the u16 at +0x08 and 0x8234CB58 dispatches on it. Only
  // 0x200 is served here, because 0x82350AF8 - the constructor for codes 1 and
  // 2 - reads a different layout and no shipped file selects it.
  container.type_code_ = Be16(base + 0x08);
  if (container.type_code_ != kSupportedTypeCode) {
    SetRefusal(refusal, NdxrRefusal::kUnsupportedCode);
    return std::nullopt;
  }

  container.record_count_ = Be16(base + 0x0A);
  if (container.record_count_ == 0) {
    SetRefusal(refusal, NdxrRefusal::kNoRecords);
    return std::nullopt;
  }

  // 0x82350F08, in 64-bit arithmetic so a hostile file cannot wrap the adds.
  const std::uint64_t a = Be32(base + 0x10);
  const std::uint64_t b = Be32(base + 0x14);
  const std::uint64_t c = Be32(base + 0x18);
  const std::uint64_t d = Be32(base + 0x1C);
  const std::uint64_t first = a + kBodyBaseBias;
  const std::uint64_t second = first + b;
  const std::uint64_t third = second + c;
  const std::uint64_t end = third + d;
  if (end > remaining) {
    SetRefusal(refusal, NdxrRefusal::kSectionOverflow);
    return std::nullopt;
  }
  container.sections_.first = static_cast<std::size_t>(first);
  container.sections_.second = static_cast<std::size_t>(second);
  // 82350f64: the third boundary is zero when the fourth extent is zero.
  container.sections_.third = (d == 0) ? 0 : static_cast<std::size_t>(third);
  container.sections_.end = static_cast<std::size_t>(end);

  // 0x823556E0 walks record_count records of kRecordStride from kRecordArrayBase.
  const std::uint64_t array_end =
      static_cast<std::uint64_t>(kRecordArrayBase) +
      static_cast<std::uint64_t>(container.record_count_) * kRecordStride;
  if (array_end > remaining) {
    SetRefusal(refusal, NdxrRefusal::kRecordOverflow);
    return std::nullopt;
  }

  container.bytes_ = base;
  container.size_ = remaining;
  return container;
}

std::optional<NdxrRecord> NdxrContainer::Record(
    std::uint16_t index) const noexcept {
  if (bytes_ == nullptr || index >= record_count_) return std::nullopt;

  const std::uint8_t* rec = bytes_ + kRecordArrayBase + static_cast<std::size_t>(index) * kRecordStride;

  NdxrRecord out;
  out.index = index;
  out.relocated = (Be16(rec + 0x26) & kRelocatedBit) != 0;
  out.descriptor_count = Be16(rec + 0x2A);
  out.descriptor_offset = Be32(rec + 0x2C);

  // rec+0x20 is relocated by [obj+0x90] - the end of the four sections - which
  // is where the string table begins. Resolve rather than relocate: this reader
  // holds a const buffer, unlike retail.
  const std::uint64_t name_at =
      static_cast<std::uint64_t>(sections_.end) + static_cast<std::uint64_t>(Be32(rec + 0x20));
  if (name_at >= size_) return std::nullopt;

  const std::uint8_t* start = bytes_ + name_at;
  const std::size_t room = size_ - static_cast<std::size_t>(name_at);
  const void* nul = std::memchr(start, 0, room);
  if (nul == nullptr) return std::nullopt;  // unterminated: refuse, do not clamp
  out.name = std::string_view(reinterpret_cast<const char*>(start),
                              static_cast<std::size_t>(static_cast<const std::uint8_t*>(nul) - start));
  return out;
}

}  // namespace ac6::retail

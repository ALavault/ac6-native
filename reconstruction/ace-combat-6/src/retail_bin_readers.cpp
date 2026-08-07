#include "ac6/retail_bin_readers.h"

#include <algorithm>
#include <cstdio>
#include <map>

namespace ac6::retail {
namespace {

constexpr char kHex[] = "0123456789abcdef";

// tag -> record word index, and the error string the branch materializes when
// its payload pointer is absent. Read off OrderBin::read.
struct OrderTag {
  std::uint32_t word;
  std::uint32_t error;  // 0 when the branch has no error path
};

const std::map<std::uint8_t, OrderTag>& order_tags() {
  static const std::map<std::uint8_t, OrderTag> tags = {
      {0, {1, 0}},          {1, {2, 0x820102C4}}, {2, {3, 0}},
      {3, {4, 0x820102F8}}, {4, {5, 0x82010324}}, {5, {6, 0x8201037C}},
      {6, {7, 0x82010350}}, {7, {8, 0}},          {8, {9, 0x820103A8}},
      {9, {10, 0}},
  };
  return tags;
}

// The unnamed 0x28 element's union: tag -> record word index. No variant can be
// named, because its subtree carries no error string at all.
const std::map<std::uint8_t, std::uint32_t>& unnamed28_tags() {
  static const std::map<std::uint8_t, std::uint32_t> tags = {
      {0, 1}, {4, 2}, {5, 3}, {6, 4}, {1, 5}, {2, 6}, {7, 8}, {8, 9},
  };
  return tags;
}

}  // namespace

BinImage::BinImage()
    : record_(kRecordBytes, 0), record_written_(kRecordBytes, 0),
      buffer_(kBufferBytes, 0), buffer_written_(kBufferBytes, 0) {}

void BinImage::write32(std::uint32_t address, std::uint32_t value) noexcept {
  std::vector<std::uint8_t>* region = nullptr;
  std::vector<std::uint8_t>* written = nullptr;
  std::size_t offset = 0;
  if (address >= kRecordBase && address < kRecordBase + kRecordBytes) {
    region = &record_;
    written = &record_written_;
    offset = address - kRecordBase;
  } else if (address >= kBufferBase && address < kBufferBase + kBufferBytes) {
    region = &buffer_;
    written = &buffer_written_;
    offset = address - kBufferBase;
  }
  if (region == nullptr || offset + 4 > region->size()) {
    overflowed_ = true;
    return;
  }
  for (unsigned index = 0; index < 4; ++index) {
    (*region)[offset + index] =
        static_cast<std::uint8_t>(value >> (8 * (3 - index)));
    (*written)[offset + index] = 1;
  }
}

void BinImage::zero(std::uint32_t address, unsigned words) noexcept {
  for (unsigned index = 0; index < words; ++index) {
    write32(address + 4 * index, 0);
  }
}

std::vector<BinImage::Run> BinImage::runs() const {
  std::vector<Run> out;
  const std::pair<std::uint32_t, const std::vector<std::uint8_t>*> regions[] = {
      {kRecordBase, &record_}, {kBufferBase, &buffer_}};
  const std::vector<std::uint8_t>* masks[] = {&record_written_, &buffer_written_};
  for (std::size_t which = 0; which < 2; ++which) {
    const std::vector<std::uint8_t>& region = *regions[which].second;
    const std::vector<std::uint8_t>& mask = *masks[which];
    std::size_t index = 0;
    while (index < region.size()) {
      if (mask[index] == 0) {
        index += 1;
        continue;
      }
      const std::size_t start = index;
      while (index < region.size() && mask[index] != 0) index += 1;
      Run run;
      run.address = regions[which].first + static_cast<std::uint32_t>(start);
      run.bytes.assign(region.begin() + static_cast<std::ptrdiff_t>(start),
                       region.begin() + static_cast<std::ptrdiff_t>(index));
      out.push_back(std::move(run));
    }
  }
  std::sort(out.begin(), out.end(),
            [](const Run& left, const Run& right) { return left.address < right.address; });
  return out;
}

std::size_t BinImage::run_count() const { return runs().size(); }

std::size_t BinImage::written_bytes() const {
  std::size_t total = 0;
  for (const Run& run : runs()) total += run.bytes.size();
  return total;
}

std::uint64_t BinImage::digest() const {
  std::string text;
  char header[32];
  for (const Run& run : runs()) {
    std::snprintf(header, sizeof(header), "%08x:%zu:", run.address, run.bytes.size());
    text += header;
    for (const std::uint8_t byte : run.bytes) {
      text.push_back(kHex[byte >> 4]);
      text.push_back(kHex[byte & 0x0F]);
    }
    text.push_back('\n');
  }
  std::uint64_t digest = 0xCBF29CE484222325ull;
  for (const char character : text) {
    digest ^= static_cast<std::uint8_t>(character);
    digest *= 0x100000001B3ull;
  }
  return digest;
}

// -- helpers -----------------------------------------------------------------

std::uint32_t BinReaders::guest(std::optional<std::size_t> offset) noexcept {
  return offset.has_value() ? kPayloadBase + static_cast<std::uint32_t>(*offset) : 0;
}

std::optional<std::size_t> BinReaders::resolve(std::size_t node, unsigned word) {
  return payload_.resolve(node, word);
}

std::optional<std::size_t> BinReaders::child_at(
    const std::vector<std::size_t>& children, std::size_t index) {
  if (index >= children.size()) return std::nullopt;
  return children[index];
}

std::uint8_t BinReaders::u8(std::size_t offset) {
  const std::optional<std::uint8_t> value = payload_.u8(offset);
  if (!value.has_value()) {
    out_of_range();
    return 0;
  }
  return *value;
}

std::int32_t BinReaders::s32(std::size_t offset) {
  const std::optional<std::uint32_t> value = payload_.u32(offset);
  if (!value.has_value()) {
    out_of_range();
    return 0;
  }
  return static_cast<std::int32_t>(*value);
}

std::uint16_t BinReaders::u16(std::size_t offset) {
  const std::optional<std::uint8_t> high = payload_.u8(offset);
  const std::optional<std::uint8_t> low = payload_.u8(offset + 1);
  if (!high.has_value() || !low.has_value()) {
    out_of_range();
    return 0;
  }
  return static_cast<std::uint16_t>(*high << 8 | *low);
}

void BinReaders::fail(std::uint32_t string_address) {
  diagnostics_.push_back({string_address});
}

void BinReaders::unmodelled(std::string_view what) {
  ok_ = false;
  failure_ = std::string("unmodelled descent: ") + std::string(what);
}

void BinReaders::out_of_range() {
  ok_ = false;
  failure_ = "payload read out of range";
}

// -- ComBin / ComTblBin ------------------------------------------------------

void BinReaders::com_read(std::uint32_t record, std::size_t node) {
  const std::optional<std::size_t> data = resolve(node, 0);
  image_.write32(record, guest(data));
  if (!data.has_value()) fail(0x820106BC);
}

std::uint32_t BinReaders::comtbl_size(std::size_t node) {
  const std::optional<std::size_t> data = resolve(node, 0);
  return data.has_value() ? static_cast<std::uint32_t>(u8(*data)) << 2 : 0;
}

void BinReaders::comtbl_read(std::uint32_t record, std::size_t node,
                             std::uint32_t buffer) {
  const std::optional<std::size_t> data = resolve(node, 0);
  image_.write32(record, guest(data));
  if (!data.has_value()) {
    fail(0x82010690);
    return;
  }
  const std::uint8_t count = u8(*data);
  if (count == 0) return;  // early exit, before the child table
  const std::vector<std::size_t> children = payload_.children(node);
  image_.write32(record + 4, buffer);
  for (std::uint8_t index = 0; index < count && ok_; ++index) {
    const std::optional<std::size_t> child = child_at(children, index);
    image_.write32(buffer + 4u * index, 0);
    if (!child.has_value() || !payload_.present(*child)) {
      fail(0x82010634);
      continue;
    }
    com_read(buffer + 4u * index, *child);
  }
}

// -- ManeuverBin -------------------------------------------------------------

void BinReaders::maneuver_read(std::uint32_t record, std::size_t node,
                               std::uint32_t buffer) {
  const std::optional<std::size_t> data = resolve(node, 0);
  image_.write32(record, guest(data));
  if (!data.has_value()) {
    fail(0x820105D4);
    return;
  }
  const std::int32_t count = s32(*data);
  const std::uint32_t comtblm = buffer;
  const std::uint32_t comtbl = buffer + static_cast<std::uint32_t>(count) * 4u;
  std::uint32_t cursor = comtbl + static_cast<std::uint32_t>(count) * 8u;
  image_.write32(record + 4, comtblm);
  image_.write32(record + 8, comtbl);

  const std::vector<std::size_t> children = payload_.children(node);
  for (std::int32_t index = 0; index < count && ok_; ++index) {
    const std::optional<std::size_t> child =
        child_at(children, static_cast<std::size_t>(index));
    image_.write32(comtblm + 4u * static_cast<std::uint32_t>(index), 0);
    image_.zero(comtbl + 8u * static_cast<std::uint32_t>(index), 2);
    if (!child.has_value() || !payload_.present(*child)) {
      fail(0x82010538);
      continue;
    }
    const std::optional<std::size_t> payload_pointer = resolve(*child, 0);
    image_.write32(comtblm + 4u * static_cast<std::uint32_t>(index),
                   guest(payload_pointer));
    if (!payload_pointer.has_value()) fail(0x82010460);

    std::optional<std::size_t> comtbl_node;
    if (resolve(*child, 1).has_value()) {
      const std::vector<std::size_t> grandchildren = payload_.children(*child);
      if (!grandchildren.empty() && payload_.present(grandchildren[0])) {
        comtbl_node = grandchildren[0];
      }
    }
    if (!comtbl_node.has_value()) {
      fail(0x8201056C);
      continue;
    }
    comtbl_read(comtbl + 8u * static_cast<std::uint32_t>(index), *comtbl_node, cursor);
    cursor += comtbl_size(*comtbl_node);
  }
}

std::uint32_t BinReaders::maneuver_size(std::size_t node) {
  const std::optional<std::size_t> data = resolve(node, 0);
  if (!data.has_value()) return 0;
  const std::int32_t count = s32(*data);
  std::uint32_t total = static_cast<std::uint32_t>(count) * 12u;
  const std::vector<std::size_t> children = payload_.children(node);
  for (std::int32_t index = 0; index < count; ++index) {
    const std::optional<std::size_t> child =
        child_at(children, static_cast<std::size_t>(index));
    if (!child.has_value() || !payload_.present(*child)) continue;
    const std::vector<std::size_t> grandchildren = payload_.children(*child);
    if (!grandchildren.empty() && payload_.present(grandchildren[0])) {
      total += comtbl_size(grandchildren[0]);
    }
  }
  return total;
}

// -- ObjBin maneuver block ---------------------------------------------------

void BinReaders::maneuver_block_read(std::uint32_t record, std::size_t node,
                                     std::uint32_t buffer) {
  const std::optional<std::size_t> data = resolve(node, 0);
  image_.write32(record, guest(data));
  std::uint32_t cursor = buffer;
  const std::vector<std::size_t> children = payload_.children(node);
  for (std::size_t index = 0; index < children.size() && index < 8 && ok_; ++index) {
    if (!payload_.present(children[index])) continue;
    image_.write32(record + 4 + 4 * static_cast<std::uint32_t>(index), cursor);
    maneuver_read(cursor, children[index], cursor + 0x0C);
    cursor += 0x0C + maneuver_size(children[index]);
  }
}

std::uint32_t BinReaders::maneuver_block_size(std::size_t node) {
  // 0x82330A30 starts at 0x60 - eight maneuver records of 0x0C - and, when slot
  // 0 is present, *overwrites* that base with size0 + 0x6C rather than adding
  // to it. The reader advances only 0x0C per present slot, so the two disagree
  // by 0x60; ObjBin::read follows the sizer, so the sizer is the layout.
  const std::vector<std::size_t> children = payload_.children(node);
  std::uint32_t total = 0x60;
  for (std::size_t index = 0; index < children.size() && index < 8; ++index) {
    if (!payload_.present(children[index])) continue;
    if (index == 0) {
      total = maneuver_size(children[index]) + 0x6C;
    } else {
      total += maneuver_size(children[index]) + 0x0C;
    }
  }
  return total;
}

// -- ObjBin param variant ----------------------------------------------------

void BinReaders::param_read(std::uint32_t record, std::size_t node,
                            std::uint32_t buffer) {
  const std::optional<std::size_t> tag_pointer = resolve(node, 0);
  image_.write32(record, guest(tag_pointer));
  const std::vector<std::size_t> children = payload_.children(node);
  std::optional<std::size_t> child;
  if (!children.empty() && payload_.present(children[0])) child = children[0];
  if (!tag_pointer.has_value()) return;
  const std::uint8_t tag = u8(*tag_pointer);
  if (tag > 2) return;  // the reader writes nothing
  image_.write32(record + 4 + 4u * tag, buffer);
  const std::optional<std::size_t> target =
      child.has_value() ? resolve(*child, 0) : std::nullopt;
  image_.write32(buffer, guest(target));
  if (tag == 1 && !target.has_value()) fail(0x82010294);
}

std::uint32_t BinReaders::param_size(std::size_t node) {
  const std::optional<std::size_t> data = resolve(node, 0);
  if (!data.has_value()) return 0;
  return u8(*data) <= 2 ? 0x10 : 0;
}

// -- OrderBin ----------------------------------------------------------------

void BinReaders::order_tag2_read(std::uint32_t record,
                                 std::optional<std::size_t> node) {
  // 0x82331AD0 needs a resolved data word, a table and a present first child
  // before it writes anything past the header. This payload never satisfies
  // that, so the check is reproduced rather than assumed: a node that did
  // descend fails here instead of passing silently.
  if (!node.has_value()) return;
  const std::optional<std::size_t> data = resolve(*node, 0);
  image_.write32(record, guest(data));
  if (!data.has_value()) return;
  if (!resolve(*node, 1).has_value()) return;
  const std::vector<std::size_t> children = payload_.children(*node);
  if (children.empty() || !payload_.present(children[0])) return;
  unmodelled("OrderBin tag 2, 0x82331D98");
}

std::uint32_t BinReaders::order_tag2_size(std::optional<std::size_t> node) {
  if (!node.has_value()) return 0;
  if (!resolve(*node, 1).has_value()) return 0;
  const std::vector<std::size_t> children = payload_.children(*node);
  if (children.empty() || !payload_.present(children[0])) return 0;
  unmodelled("OrderBin tag 2 sizer");
  return 0;
}

void BinReaders::order_read(std::uint32_t record, std::size_t node,
                            std::uint32_t buffer) {
  const std::optional<std::size_t> data = resolve(node, 0);
  image_.write32(record, guest(data));
  if (!data.has_value()) fail(0x82010438);
  if (!resolve(node, 1).has_value()) fail(0x8201040C);

  // The reader takes child 0 only, and only when the table is non-empty and
  // that child is present; otherwise it carries a null child forward.
  const std::vector<std::size_t> children = payload_.children(node);
  std::optional<std::size_t> child;
  if (!children.empty() && payload_.present(children[0])) child = children[0];

  if (!data.has_value()) return;
  const std::uint8_t tag = u8(*data);
  const auto entry = order_tags().find(tag);
  if (entry == order_tags().end()) return;  // the reader writes nothing at all

  if (tag == 2) {
    image_.zero(buffer, 2);
    image_.write32(record + 4 * entry->second.word, buffer);
    order_tag2_read(buffer, child);
    return;
  }
  image_.write32(buffer, 0);
  image_.write32(record + 4 * entry->second.word, buffer);
  const std::optional<std::size_t> target =
      child.has_value() ? resolve(*child, 0) : std::nullopt;
  image_.write32(buffer, guest(target));
  if (entry->second.error != 0 && !target.has_value()) fail(entry->second.error);
}

std::uint32_t BinReaders::order_size(std::optional<std::size_t> node) {
  // The retail sizer would dereference a null data pointer for an absent order
  // child. No order child is absent in this payload, so the path is dead;
  // returning 0 keeps the model total rather than guessing a fault.
  if (!node.has_value()) return 0;
  const std::optional<std::size_t> data = resolve(*node, 0);
  if (!data.has_value()) {
    fail(0x820103D8);
    return 0;
  }
  const std::uint8_t tag = u8(*data);
  if (tag == 2) {
    const std::vector<std::size_t> children = payload_.children(*node);
    std::optional<std::size_t> child;
    if (!children.empty() && payload_.present(children[0])) child = children[0];
    return 8 + order_tag2_size(child);
  }
  return order_tags().count(tag) != 0 ? 4 : 0;
}

// -- ActBin / SetBin ---------------------------------------------------------

void BinReaders::act_read(std::uint32_t record, std::size_t node,
                          std::uint32_t buffer) {
  const std::optional<std::size_t> data = resolve(node, 0);
  image_.write32(record, guest(data));
  if (!data.has_value()) {
    fail(0x8201026C);
    return;
  }
  const std::uint8_t count = u8(*data);
  if (count == 0) return;  // early exit, before the child table
  image_.write32(record + 4, buffer);
  std::uint32_t cursor = buffer + count * 0x2Cu;
  if (!resolve(node, 1).has_value()) fail(0x82010244);
  const std::vector<std::size_t> children = payload_.children(node);

  for (std::uint8_t index = 0; index < count && ok_; ++index) {
    const std::uint32_t element = buffer + index * 0x2Cu;
    image_.zero(element, 11);
    std::optional<std::size_t> child = child_at(children, index);
    if (child.has_value() && !payload_.present(*child)) child.reset();
    if (!child.has_value()) {
      fail(0x82010218);
      continue;
    }
    order_read(element, *child, cursor);
    cursor += order_size(child);
  }
}

std::uint32_t BinReaders::act_size(std::optional<std::size_t> node) {
  if (!node.has_value()) return 0;
  const std::optional<std::size_t> data = resolve(*node, 0);
  if (!data.has_value()) {
    fail(0x820101E4);
    return 0;
  }
  const std::uint8_t count = u8(*data);
  if (count == 0) return 0;
  // The reader writes 0x08 per act element; this sizer reserves 0x2C. The
  // over-reservation is retail's, recorded rather than corrected.
  std::uint32_t total = count * 0x2Cu;
  if (!resolve(*node, 1).has_value()) fail(0x820101B0);
  const std::vector<std::size_t> children = payload_.children(*node);
  for (std::uint8_t index = 0; index < count; ++index) {
    std::optional<std::size_t> child = child_at(children, index);
    if (child.has_value() && !payload_.present(*child)) child.reset();
    if (!child.has_value()) fail(0x82010178);
    total += order_size(child);
  }
  return total;
}

void BinReaders::set_read(std::uint32_t record, std::size_t node,
                          std::uint32_t buffer) {
  const std::optional<std::size_t> data = resolve(node, 0);
  image_.write32(record, guest(data));
  if (!data.has_value()) {
    fail(0x82010074);
    return;
  }
  const std::uint8_t count = u8(*data);
  if (count == 0) return;
  image_.write32(record + 4, buffer);
  std::uint32_t cursor = buffer + count * 8u;
  if (!resolve(node, 1).has_value()) fail(0x8201004C);
  const std::vector<std::size_t> children = payload_.children(node);

  for (std::uint8_t index = 0; index < count && ok_; ++index) {
    const std::uint32_t element = buffer + index * 8u;
    image_.zero(element, 2);
    std::optional<std::size_t> child = child_at(children, index);
    if (child.has_value() && !payload_.present(*child)) child.reset();
    if (!child.has_value()) {
      fail(0x82010020);
      continue;
    }
    act_read(element, *child, cursor);
    cursor += act_size(child);
  }
}

// -- the unnamed 0x28 element and its list -----------------------------------

void BinReaders::unnamed28_read(std::uint32_t record,
                                std::optional<std::size_t> node,
                                std::uint32_t buffer) {
  if (!node.has_value()) return;
  const std::optional<std::size_t> data = resolve(*node, 0);
  image_.write32(record, guest(data));
  if (!data.has_value()) return;
  if (!resolve(*node, 1).has_value()) return;
  const std::vector<std::size_t> children = payload_.children(*node);
  if (children.empty() || !payload_.present(children[0])) return;
  const std::uint8_t tag = u8(*data);
  const auto entry = unnamed28_tags().find(tag);
  if (entry == unnamed28_tags().end()) return;
  if (tag == 2) {
    unmodelled("unnamed 0x28 tag 2, 0x823308E0");
    return;
  }
  image_.write32(buffer, 0);
  image_.write32(record + 4 * entry->second, buffer);
  image_.write32(buffer, guest(resolve(children[0], 0)));
}

std::uint32_t BinReaders::unnamed28_size(std::optional<std::size_t> node) {
  if (!node.has_value()) return 0;
  const std::optional<std::size_t> data = resolve(*node, 0);
  if (!data.has_value() || !resolve(*node, 1).has_value()) return 0;
  const std::vector<std::size_t> children = payload_.children(*node);
  if (children.empty() || !payload_.present(children[0])) return 0;
  const std::uint8_t tag = u8(*data);
  if (tag == 2) {
    unmodelled("unnamed 0x28 tag 2 sizer");
    return 0;
  }
  return unnamed28_tags().count(tag) != 0 ? 4 : 0;
}

void BinReaders::unnamed28_list_read(std::uint32_t record, std::size_t node,
                                     std::uint32_t buffer) {
  const std::optional<std::size_t> data = resolve(node, 0);
  image_.write32(record, guest(data));
  if (!data.has_value() || !resolve(node, 1).has_value()) return;
  const std::uint8_t count = u8(*data);
  image_.write32(record + 4, buffer);
  std::uint32_t cursor = buffer + count * 0x28u;
  const std::vector<std::size_t> children = payload_.children(node);
  for (std::uint8_t index = 0; index < count; ++index) {
    image_.zero(buffer + index * 0x28u, 10);
  }
  for (std::uint8_t index = 0; index < count && ok_; ++index) {
    std::optional<std::size_t> child = child_at(children, index);
    if (child.has_value() && !payload_.present(*child)) child.reset();
    unnamed28_read(buffer + index * 0x28u, child, cursor);
    cursor += unnamed28_size(child);
  }
}

std::uint32_t BinReaders::unnamed28_list_size(std::size_t node) {
  // 0x8232EC08 rounds its total up to a multiple of 16. It is the only sizer in
  // the family that aligns, and it is worth 8 bytes on a two-element list.
  const std::optional<std::size_t> data = resolve(node, 0);
  if (!data.has_value() || !resolve(node, 1).has_value()) return 0;
  const std::uint8_t count = u8(*data);
  std::uint32_t total = count * 0x28u;
  const std::vector<std::size_t> children = payload_.children(node);
  for (std::uint8_t index = 0; index < count; ++index) {
    std::optional<std::size_t> child = child_at(children, index);
    if (child.has_value() && !payload_.present(*child)) child.reset();
    total += unnamed28_size(child);
  }
  return (total + 0xF) & ~0xFu;
}

// -- SubMisBin / SubMisTblBin / RadioTblBin ----------------------------------

void BinReaders::submis_read(std::uint32_t record, std::size_t node,
                             std::uint32_t buffer) {
  image_.write32(record, guest(resolve(node, 0)));
  if (!resolve(node, 1).has_value()) {
    fail(0x8200FE9C);
    return;
  }
  const std::vector<std::size_t> children = payload_.children(node);

  if (!children.empty() && payload_.present(children[0])) {
    image_.write32(buffer, 0);
    image_.write32(record + 4, buffer);
    const std::optional<std::size_t> target = resolve(children[0], 0);
    image_.write32(buffer, guest(target));
    if (!target.has_value()) fail(0x8200FE38);
    buffer += 0x10;
  }
  if (children.size() > 1 && payload_.present(children[1])) {
    image_.zero(buffer, 2);
    image_.write32(record + 8, buffer);
    unnamed28_list_read(buffer, children[1], buffer + 8);
  }
}

std::uint32_t BinReaders::submis_size(std::size_t node) {
  if (!resolve(node, 1).has_value()) {
    fail(0x8200FE64);
    return 0;
  }
  std::uint32_t total = 0;
  const std::vector<std::size_t> children = payload_.children(node);
  if (!children.empty() && payload_.present(children[0])) total = 0x10;
  if (children.size() > 1 && payload_.present(children[1])) {
    total += unnamed28_list_size(children[1]) + 8;
  }
  return total;
}

void BinReaders::submistbl_read(std::uint32_t record, std::size_t node,
                                std::uint32_t buffer) {
  const std::optional<std::size_t> data = resolve(node, 0);
  image_.write32(record, guest(data));
  if (!data.has_value()) {
    fail(0x8200F6BC);
    return;
  }
  const std::uint8_t count = u8(*data);
  if (count == 0) return;
  image_.write32(record + 4, buffer);
  std::uint32_t cursor = buffer + count * 0x10u;
  if (!resolve(node, 1).has_value()) fail(0x8200F68C);
  const std::vector<std::size_t> children = payload_.children(node);

  for (std::uint8_t index = 0; index < count && ok_; ++index) {
    const std::uint32_t element = buffer + index * 0x10u;
    std::optional<std::size_t> child = child_at(children, index);
    if (child.has_value() && !payload_.present(*child)) child.reset();
    if (!child.has_value()) fail(0x8200F65C);
    image_.zero(element, 3);
    if (!child.has_value()) continue;
    submis_read(element, *child, cursor);
    cursor += submis_size(*child);
  }
}

void BinReaders::radiotbl_read(std::uint32_t record, std::size_t node,
                               std::uint32_t buffer) {
  const std::optional<std::size_t> data = resolve(node, 0);
  image_.write32(record, guest(data));
  if (!data.has_value()) {
    fail(0x8200F574);
    return;
  }
  const std::uint16_t count = u16(*data);
  if (count == 0) return;
  if (!resolve(node, 1).has_value()) fail(0x8200F544);
  image_.write32(record + 4, buffer);
  const std::vector<std::size_t> children = payload_.children(node);
  for (std::uint16_t index = 0; index < count && ok_; ++index) {
    const std::uint32_t element = buffer + index * 0x10u;
    image_.write32(element, 0);
    std::optional<std::size_t> child = child_at(children, index);
    if (child.has_value() && !payload_.present(*child)) child.reset();
    if (!child.has_value()) fail(0x8200F50C);
    // 0x8232C7B0 writes one word: the resolved payload, or zero. The remaining
    // twelve bytes of each 0x10 entry are never touched.
    image_.write32(element,
                   child.has_value() ? guest(resolve(*child, 0)) : 0);
  }
}

// -- ObjBin ------------------------------------------------------------------

// ObjBin::read, 0x82330158.
void BinReaders::obj_read(std::uint32_t record, std::size_t node,
                          std::uint32_t buffer) {
  const std::optional<std::size_t> data = resolve(node, 0);
  image_.write32(record, guest(data));
  if (!data.has_value()) fail(0x82010150);
  const std::vector<std::size_t> children = payload_.children(node);
  std::uint32_t cursor = buffer;

  if (!children.empty() && payload_.present(children[0])) {
    image_.zero(cursor, 4);
    image_.write32(record + 0x04, cursor);
    param_read(cursor, children[0], cursor + 0x10);
    cursor += 0x10 + param_size(children[0]);
  }
  if (children.size() > 1 && payload_.present(children[1])) {
    image_.zero(cursor, 9);
    image_.write32(record + 0x08, cursor);
    maneuver_block_read(cursor, children[1], cursor + 0x24);
    cursor += 0x24 + maneuver_block_size(children[1]);
  }
  const std::pair<std::size_t, std::uint32_t> slots[] = {
      {2, 0x0C}, {3, 0x10}, {4, 0x14}, {5, 0x18}};
  for (const auto& [index, offset] : slots) {
    if (children.size() <= index || !payload_.present(children[index])) continue;
    image_.write32(cursor, 0);
    image_.write32(record + offset, cursor);
    image_.write32(cursor, guest(resolve(children[index], 0)));
    cursor += 0x10;
  }
  if (children.size() > 6 && payload_.present(children[6])) {
    image_.write32(cursor, 0);
    image_.write32(record + 0x1C, cursor);
    image_.write32(cursor, guest(resolve(children[6], 0)));
  }
}

// -- entry points ------------------------------------------------------------

// The entry points the scenario root reader 0x82249718 dispatches to.
bool BinReaders::run(std::string_view klass, std::size_t node) {
  if (klass == "ComBin") {
    com_read(kRecordBase, node);
  } else if (klass == "ComTblBin") {
    comtbl_read(kRecordBase, node, kBufferBase);
  } else if (klass == "ManeuverBin") {
    maneuver_read(kRecordBase, node, kBufferBase);
  } else if (klass == "OrderBin") {
    order_read(kRecordBase, node, kBufferBase);
  } else if (klass == "ActBin") {
    act_read(kRecordBase, node, kBufferBase);
  } else if (klass == "SetBin") {
    set_read(kRecordBase, node, kBufferBase);
  } else if (klass == "SubMisTblBin") {
    submistbl_read(kRecordBase, node, kBufferBase);
  } else if (klass == "SubMisBin") {
    submis_read(kRecordBase, node, kBufferBase);
  } else if (klass == "RadioTblBin") {
    radiotbl_read(kRecordBase, node, kBufferBase);
  } else if (klass == "ObjBin") {
    obj_read(kRecordBase, node, kBufferBase);
  } else {
    ok_ = false;
    failure_ = "unknown reader class";
  }
  if (ok_ && image_.overflowed()) {
    ok_ = false;
    failure_ = "write outside the synthetic regions";
  }
  return ok_;
}

}  // namespace ac6::retail

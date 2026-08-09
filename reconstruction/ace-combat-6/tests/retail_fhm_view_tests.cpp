#include "ac6/retail_fhm_view.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <span>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char* message) {
  if (!condition) {
    std::printf("FAIL  %s\n", message);
    ++failures;
  }
}

void put32(std::vector<std::uint8_t>& bytes, std::size_t at,
           std::uint32_t value) {
  bytes[at] = static_cast<std::uint8_t>(value >> 24);
  bytes[at + 1] = static_cast<std::uint8_t>(value >> 16);
  bytes[at + 2] = static_cast<std::uint8_t>(value >> 8);
  bytes[at + 3] = static_cast<std::uint8_t>(value);
}

std::vector<std::uint8_t> make_fhm(
    const std::vector<std::vector<std::uint8_t>>& children) {
  const std::size_t table_end = 0x14 + children.size() * 16;
  std::vector<std::uint8_t> bytes(table_end, 0);
  std::memcpy(bytes.data(), "FHM ", 4);
  bytes[4] = 1;
  bytes[5] = 1;
  bytes[7] = 0x10;
  put32(bytes, 0x10, static_cast<std::uint32_t>(children.size()));
  for (std::size_t index = 0; index < children.size(); ++index) {
    if (children[index].empty()) continue;
    put32(bytes, 0x14 + index * 4, static_cast<std::uint32_t>(bytes.size()));
    put32(bytes, 0x14 + children.size() * 4 + index * 4,
          static_cast<std::uint32_t>(children[index].size()));
    bytes.insert(bytes.end(), children[index].begin(), children[index].end());
  }
  return bytes;
}

}  // namespace

int main() {
  using ac6::retail::RetailFhmView;

  const std::vector<std::uint8_t> nested =
      make_fhm({{'N', 'D', 'X', 'R'}, {'N', 'T', 'X', 'R'}});
  const std::vector<std::uint8_t> bytes =
      make_fhm({{'A', 'C', 'E', '6'}, nested, {}});
  const std::optional<RetailFhmView> root = RetailFhmView::open(bytes);
  check(root.has_value(), "a bounded native-endian FHM opens");
  if (!root.has_value()) return 1;
  check(root->child_count() == 3, "the declared child count is retained");
  check(root->child(0).has_value() && root->child(0)->size() == 4,
        "a live child exposes its exact array-1 length");
  check(root->child_length(2).has_value() && *root->child_length(2) == 0,
        "an empty slot remains distinguishable from an invalid index");
  check(!root->child(2).has_value(), "an empty slot is not a payload span");
  check(!root->child_length(3).has_value(), "an out-of-range slot fails closed");
  check(root->nested(1).has_value() && root->nested(1)->child_count() == 2,
        "a nested FHM is parsed through the same bounds");
  check(!root->nested(0).has_value(), "a non-FHM child is not treated as nested");

  const std::array<std::uint32_t, 2> path{1, 1};
  const std::optional<std::span<const std::uint8_t>> leaf =
      root->descendant(path);
  check(leaf.has_value() && leaf->size() == 4 && (*leaf)[0] == 'N' &&
            (*leaf)[1] == 'T',
        "a descendant path returns the exact nested child");
  const std::array<std::uint32_t, 2> bad_path{0, 1};
  check(!root->descendant(bad_path).has_value(),
        "a path cannot descend through an opaque child");

  std::vector<std::uint8_t> invalid = bytes;
  invalid[0] = 'X';
  check(!RetailFhmView::open(invalid).has_value(), "wrong magic fails closed");
  invalid = bytes;
  invalid.resize(0x20);
  check(!RetailFhmView::open(invalid).has_value(),
        "a truncated parallel table fails closed");
  invalid = bytes;
  put32(invalid, 0x14 + 3 * 4, 0xFFFFFFFFu);
  check(!RetailFhmView::open(invalid).has_value(),
        "a live child extent outside the container fails closed");
  invalid = bytes;
  invalid[5] = 0;
  check(!RetailFhmView::open(invalid).has_value(),
        "the unsupported swapped retail path fails closed");

  if (failures == 0) std::printf("retail FHM view OK\n");
  return failures == 0 ? 0 : 1;
}

#include "ac6/retail_scene_tcam.h"

#include <algorithm>
#include <limits>
#include <optional>
#include <utility>

#include "ac6/retail_fhm_view.h"

namespace ac6::retail {
namespace {

constexpr std::size_t kMaximumFhmDepth = 64;
constexpr std::size_t kMaximumFhmContainers = 100000;
constexpr std::uint16_t kRetailFhmHeaderSize = 0x10;
constexpr std::uint32_t kTcamGyzHeaderSize = 0x20;
constexpr std::uint32_t kTcamGyzTag = 0x00011E00;
constexpr std::uint32_t kMinimumTcamRecordTable = 0x50;

std::uint16_t be16(const std::uint8_t* bytes) noexcept {
  return static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(bytes[0]) << 8u) | bytes[1]);
}

std::uint32_t be32(const std::uint8_t* bytes) noexcept {
  return (static_cast<std::uint32_t>(bytes[0]) << 24u) |
         (static_cast<std::uint32_t>(bytes[1]) << 16u) |
         (static_cast<std::uint32_t>(bytes[2]) << 8u) |
         static_cast<std::uint32_t>(bytes[3]);
}

bool has_magic(std::span<const std::uint8_t> bytes,
               std::string_view magic) noexcept {
  return bytes.size() >= magic.size() &&
         std::equal(magic.begin(), magic.end(), bytes.begin());
}

bool is_tcam_path(std::string_view path) noexcept {
  const std::size_t slash = path.rfind('/');
  const std::string_view name =
      slash == std::string_view::npos ? path : path.substr(slash + 1);
  return name.size() >= 8 && name.starts_with("Tcam") && name.ends_with(".mop");
}

bool valid_scene_path(std::span<const std::uint8_t> record, std::string& path) {
  const auto terminator = std::find(record.begin(), record.end(), 0u);
  if (terminator == record.end()) return false;
  const std::size_t length =
      static_cast<std::size_t>(terminator - record.begin());
  constexpr std::string_view kPrefix = "Scene/";
  if (length < kPrefix.size() ||
      !std::equal(kPrefix.begin(), kPrefix.end(), record.begin()) ||
      !std::all_of(
          terminator + 1, record.end(),
          [](std::uint8_t byte) { return byte == 0; })) {
    return false;
  }

  for (std::size_t index = 0; index < length; ++index) {
    const std::uint8_t byte = record[index];
    if (byte < 0x20 || byte > 0x7E || byte == '\\') return false;
  }

  // Reject empty, current and parent components without assigning semantics
  // to any of the other printable path bytes.
  std::size_t component_begin = 0;
  for (std::size_t index = 0; index <= length; ++index) {
    if (index != length && record[index] != '/') continue;
    const std::size_t component_size = index - component_begin;
    if (component_size == 0 ||
        (component_size == 1 && record[component_begin] == '.') ||
        (component_size == 2 && record[component_begin] == '.' &&
         record[component_begin + 1] == '.')) {
      return false;
    }
    component_begin = index + 1;
  }

  path.assign(reinterpret_cast<const char*>(record.data()), length);
  return true;
}

std::optional<std::vector<std::string>> parse_scene_paths(
    std::span<const std::uint8_t> bytes, std::uint32_t expected_count) {
  if (expected_count == 0 ||
      expected_count > std::numeric_limits<std::size_t>::max() /
                           kRetailScenePathRecordBytes ||
      bytes.size() != static_cast<std::size_t>(expected_count) *
                          kRetailScenePathRecordBytes) {
    return std::nullopt;
  }

  std::vector<std::string> paths;
  paths.reserve(expected_count);
  for (std::uint32_t index = 0; index < expected_count; ++index) {
    std::string path;
    if (!valid_scene_path(bytes.subspan(static_cast<std::size_t>(index) *
                                            kRetailScenePathRecordBytes,
                                        kRetailScenePathRecordBytes),
                          path)) {
      return std::nullopt;
    }
    paths.push_back(std::move(path));
  }
  return paths;
}

}  // namespace

std::optional<RetailTcamMopView> RetailTcamMopView::open(
    std::span<const std::uint8_t> bytes) noexcept {
  constexpr std::size_t kWrapperSizeField = 0x20;
  constexpr std::size_t kWrapperGyzOffsetField = 0x30;
  if (bytes.size() < kWrapperGyzOffsetField + sizeof(std::uint32_t)) {
    return std::nullopt;
  }

  const std::uint32_t declared_size = be32(bytes.data() + kWrapperSizeField);
  const std::uint32_t gyz_offset = be32(bytes.data() + kWrapperGyzOffsetField);
  if (gyz_offset < kWrapperGyzOffsetField + sizeof(std::uint32_t) ||
      gyz_offset > bytes.size() || declared_size < kMinimumTcamRecordTable ||
      declared_size != bytes.size() - gyz_offset) {
    return std::nullopt;
  }

  const std::span<const std::uint8_t> gyz =
      bytes.subspan(gyz_offset, declared_size);
  if (!has_magic(gyz, std::string_view("GYZ\0", 4)) ||
      be32(gyz.data() + 0x08) != declared_size ||
      be32(gyz.data() + 0x0C) != kTcamGyzHeaderSize ||
      be32(gyz.data() + 0x20) != kTcamGyzTag ||
      be32(gyz.data() + 0x2C) != kRetailTcamRecordCount) {
    return std::nullopt;
  }

  const std::uint32_t record_table = be32(gyz.data() + 0x30);
  constexpr std::uint32_t kRecordBytes = static_cast<std::uint32_t>(
      kRetailTcamRecordCount * kRetailTcamRecordBytes);
  if (record_table < kMinimumTcamRecordTable || record_table > declared_size ||
      kRecordBytes > declared_size - record_table) {
    return std::nullopt;
  }
  const std::uint32_t data_floor = record_table + kRecordBytes;
  for (std::size_t index = 0; index < kRetailTcamRecordCount; ++index) {
    const std::size_t record =
        static_cast<std::size_t>(record_table) + index * kRetailTcamRecordBytes;
    for (const std::size_t field : {std::size_t{0x14}, std::size_t{0x18}}) {
      const std::uint32_t data_offset = be32(gyz.data() + record + field);
      if (data_offset < data_floor || data_offset >= declared_size) {
        return std::nullopt;
      }
    }
  }

  RetailTcamMopView view;
  view.bytes_ = bytes;
  view.gyz_offset_ = gyz_offset;
  view.gyz_size_ = declared_size;
  view.record_table_ = record_table;
  return view;
}

std::optional<std::span<const std::uint8_t>> RetailTcamMopView::record_bytes(
    std::size_t index) const noexcept {
  if (index >= kRetailTcamRecordCount) return std::nullopt;
  const std::size_t offset = static_cast<std::size_t>(gyz_offset_) +
                             record_table_ + index * kRetailTcamRecordBytes;
  return bytes_.subspan(offset, kRetailTcamRecordBytes);
}

std::optional<std::array<std::uint32_t, 2>>
RetailTcamMopView::record_data_offsets(std::size_t index) const noexcept {
  const std::optional<std::span<const std::uint8_t>> record =
      record_bytes(index);
  if (!record.has_value()) return std::nullopt;
  return std::array<std::uint32_t, 2>{be32(record->data() + 0x14),
                                      be32(record->data() + 0x18)};
}

class RetailSceneTcamScanner final {
 public:
  explicit RetailSceneTcamScanner(std::span<const std::uint8_t> payload)
      : payload_(payload) {}

  std::optional<RetailSceneTcamCatalog> run() {
    if (!walk(payload_, 0, std::nullopt)) return std::nullopt;
    return std::move(catalog_);
  }

 private:
  std::optional<RetailFhmView> open_strict_fhm(
      std::span<const std::uint8_t> bytes) {
    if (++container_count_ > kMaximumFhmContainers) return std::nullopt;
    const std::optional<RetailFhmView> view = RetailFhmView::open(bytes);
    if (!view.has_value() || bytes.size() < 0x18 ||
        be16(bytes.data() + 6) != kRetailFhmHeaderSize) {
      return std::nullopt;
    }

    const std::size_t table_end =
        0x14 + static_cast<std::size_t>(view->child_count()) * 16;
    if (table_end > bytes.size()) return std::nullopt;
    std::size_t previous_end = table_end;
    for (std::uint32_t index = 0; index < view->child_count(); ++index) {
      const std::optional<std::uint32_t> length = view->child_length(index);
      if (!length.has_value()) return std::nullopt;
      // A zero-size slot is metadata-only. In particular, its offset word is
      // opaque and must not be interpreted or constrained.
      if (*length == 0) continue;
      const std::optional<std::span<const std::uint8_t>> child =
          view->child(index);
      if (!child.has_value() || child->size() != *length ||
          child->data() < bytes.data() ||
          child->data() > bytes.data() + bytes.size()) {
        return std::nullopt;
      }
      const std::size_t offset =
          static_cast<std::size_t>(child->data() - bytes.data());
      if (offset < table_end || offset < previous_end ||
          child->size() > bytes.size() - offset) {
        return std::nullopt;
      }
      previous_end = offset + child->size();
    }
    return view;
  }

  bool add_scene(const RetailFhmView& container, std::uint32_t scene_index,
                 std::span<const std::uint8_t> scene_bytes,
                 std::vector<std::optional<RetailFhmView>>& parsed_children) {
    if (scene_index < 2) return false;
    const std::optional<std::span<const std::uint8_t>> state =
        container.child(scene_index - 2);
    const std::optional<std::span<const std::uint8_t>> resource_bytes =
        container.child(scene_index - 1);
    if (!state.has_value() || state->size() < 16 ||
        !has_magic(*state, std::string_view("NFICCUT\0", 8)) ||
        !resource_bytes.has_value() || !has_magic(*resource_bytes, "FHM ")) {
      return false;
    }

    std::optional<RetailFhmView>& cached_resources =
        parsed_children[scene_index - 1];
    if (!cached_resources.has_value()) {
      cached_resources = open_strict_fhm(*resource_bytes);
      if (!cached_resources.has_value()) return false;
    }
    const RetailFhmView& resources = *cached_resources;
    const std::optional<std::vector<std::string>> paths =
        parse_scene_paths(scene_bytes, resources.child_count());
    if (!paths.has_value()) return false;

    // Declared cardinality is exact: no zero-size resource slot may stand in
    // for a path and no live resource may sit outside the path table.
    for (std::uint32_t index = 0; index < resources.child_count(); ++index) {
      const std::optional<std::uint32_t> length = resources.child_length(index);
      if (!length.has_value() || *length == 0 ||
          !resources.child(index).has_value()) {
        return false;
      }
    }

    if (catalog_.scene_tables_ == std::numeric_limits<std::size_t>::max() ||
        paths->size() >
            std::numeric_limits<std::size_t>::max() - catalog_.scene_paths_) {
      return false;
    }
    ++catalog_.scene_tables_;
    catalog_.scene_paths_ += paths->size();

    for (std::uint32_t index = 0; index < resources.child_count(); ++index) {
      const std::string& retail_path = (*paths)[index];
      if (!is_tcam_path(retail_path)) continue;
      if (catalog_.find_exact(retail_path).has_value()) return false;
      const std::optional<std::span<const std::uint8_t>> bytes =
          resources.child(index);
      if (!bytes.has_value() || !RetailTcamMopView::open(*bytes).has_value() ||
          bytes->data() < payload_.data() ||
          bytes->data() > payload_.data() + payload_.size()) {
        return false;
      }
      const std::size_t payload_offset =
          static_cast<std::size_t>(bytes->data() - payload_.data());
      if (bytes->size() > payload_.size() - payload_offset) return false;

      RetailSceneTcamResource resource;
      resource.path = retail_path;
      resource.fhm_path = fhm_path_;
      resource.fhm_path.push_back(scene_index - 1);
      resource.fhm_path.push_back(index);
      resource.payload_offset = payload_offset;
      resource.size = bytes->size();
      resource.sha256 = sha256_bytes(*bytes);
      catalog_.resources_.push_back(std::move(resource));
    }
    return true;
  }

  bool walk(std::span<const std::uint8_t> bytes, std::size_t depth,
            std::optional<RetailFhmView> preparsed) {
    if (depth > kMaximumFhmDepth) return false;
    if (!preparsed.has_value()) preparsed = open_strict_fhm(bytes);
    if (!preparsed.has_value()) return false;
    const RetailFhmView& container = *preparsed;
    std::vector<std::optional<RetailFhmView>> parsed_children(
        container.child_count());

    for (std::uint32_t index = 0; index < container.child_count(); ++index) {
      const std::optional<std::span<const std::uint8_t>> child =
          container.child(index);
      if (child.has_value() && has_magic(*child, "Scen") &&
          !add_scene(container, index, *child, parsed_children)) {
        return false;
      }
    }

    for (std::uint32_t index = 0; index < container.child_count(); ++index) {
      const std::optional<std::span<const std::uint8_t>> child =
          container.child(index);
      if (!child.has_value() || !has_magic(*child, "FHM ")) continue;
      fhm_path_.push_back(index);
      const bool accepted =
          walk(*child, depth + 1, std::move(parsed_children[index]));
      fhm_path_.pop_back();
      if (!accepted) return false;
    }
    return true;
  }

  std::span<const std::uint8_t> payload_;
  std::size_t container_count_{};
  std::vector<std::uint32_t> fhm_path_;
  RetailSceneTcamCatalog catalog_;
};

std::optional<RetailSceneTcamCatalog> RetailSceneTcamCatalog::scan(
    std::span<const std::uint8_t> payload) noexcept {
  try {
    return RetailSceneTcamScanner(payload).run();
  } catch (...) {
    // Allocation and length failures are still parser failures. Untrusted
    // payload metadata never escapes as an exception or a partial catalogue.
    return std::nullopt;
  }
}

const RetailSceneTcamResource* RetailSceneTcamCatalog::resource(
    std::size_t index) const noexcept {
  return index < resources_.size() ? &resources_[index] : nullptr;
}

std::optional<std::size_t> RetailSceneTcamCatalog::find_exact(
    std::string_view path) const noexcept {
  for (std::size_t index = 0; index < resources_.size(); ++index) {
    if (resources_[index].path == path) return index;
  }
  return std::nullopt;
}

}  // namespace ac6::retail

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "ac6/retail_campaign_bundle.h"
#include "ac6/retail_content.h"
#include "ac6/retail_fhm_view.h"
#include "ac6/retail_scene_tcam.h"
#include "ac6/sha256.h"

namespace {

void require(bool condition, const char* expression, int line) {
  if (!condition) {
    std::fprintf(stderr, "REQUIRE failed at line %d: %s\n", line, expression);
    std::abort();
  }
}
#define REQUIRE(value) require((value), #value, __LINE__)

void put_be32(std::vector<std::uint8_t>& bytes, std::size_t offset,
              std::uint32_t value) {
  REQUIRE(offset <= bytes.size() && bytes.size() - offset >= 4);
  bytes[offset] = static_cast<std::uint8_t>(value >> 24u);
  bytes[offset + 1] = static_cast<std::uint8_t>(value >> 16u);
  bytes[offset + 2] = static_cast<std::uint8_t>(value >> 8u);
  bytes[offset + 3] = static_cast<std::uint8_t>(value);
}

void put_be16(std::vector<std::uint8_t>& bytes, std::size_t offset,
              std::uint16_t value) {
  REQUIRE(offset <= bytes.size() && bytes.size() - offset >= 2);
  bytes[offset] = static_cast<std::uint8_t>(value >> 8u);
  bytes[offset + 1] = static_cast<std::uint8_t>(value);
}

std::uint16_t read_be16(std::span<const std::uint8_t> bytes,
                        std::size_t offset) {
  REQUIRE(offset <= bytes.size() && bytes.size() - offset >= 2);
  return static_cast<std::uint16_t>(
      (static_cast<std::uint16_t>(bytes[offset]) << 8u) | bytes[offset + 1]);
}

struct FhmSlot final {
  std::vector<std::uint8_t> bytes;
  std::uint32_t empty_offset{};
};

std::vector<std::uint8_t> make_fhm(std::vector<FhmSlot> slots) {
  REQUIRE(!slots.empty());
  const std::size_t table_end = 0x14 + slots.size() * 16;
  std::vector<std::uint8_t> bytes(table_end, 0);
  std::memcpy(bytes.data(), "FHM ", 4);
  bytes[4] = 1;
  bytes[5] = 1;
  bytes[7] = 0x10;
  put_be32(bytes, 0x10, static_cast<std::uint32_t>(slots.size()));
  for (std::size_t index = 0; index < slots.size(); ++index) {
    if (slots[index].bytes.empty()) {
      put_be32(bytes, 0x14 + index * 4, slots[index].empty_offset);
      continue;
    }
    put_be32(bytes, 0x14 + index * 4, static_cast<std::uint32_t>(bytes.size()));
    put_be32(bytes, 0x14 + slots.size() * 4 + index * 4,
             static_cast<std::uint32_t>(slots[index].bytes.size()));
    bytes.insert(bytes.end(), slots[index].bytes.begin(),
                 slots[index].bytes.end());
  }
  return bytes;
}

std::vector<std::uint8_t> make_nfic_cut() {
  std::vector<std::uint8_t> bytes(16, 0);
  std::memcpy(bytes.data(), "NFICCUT", 7);
  return bytes;
}

std::vector<std::uint8_t> make_scene(std::span<const std::string_view> paths) {
  REQUIRE(!paths.empty());
  std::vector<std::uint8_t> bytes(
      paths.size() * ac6::retail::kRetailScenePathRecordBytes, 0);
  for (std::size_t index = 0; index < paths.size(); ++index) {
    REQUIRE(paths[index].size() < ac6::retail::kRetailScenePathRecordBytes);
    std::copy(
        paths[index].begin(), paths[index].end(),
        bytes.begin() + static_cast<std::ptrdiff_t>(
                            index * ac6::retail::kRetailScenePathRecordBytes));
  }
  return bytes;
}

std::vector<std::uint8_t> make_tcam_mop() {
  constexpr std::uint32_t kGyzOffset = 0x40;
  constexpr std::uint32_t kGyzSize = 0x100;
  constexpr std::uint32_t kRecordTable = 0x50;
  constexpr std::uint32_t kDataFloor = 0xE0;
  std::vector<std::uint8_t> bytes(kGyzOffset + kGyzSize, 0);
  put_be32(bytes, 0x20, kGyzSize);
  put_be32(bytes, 0x30, kGyzOffset);
  std::memcpy(bytes.data() + kGyzOffset, "GYZ", 3);
  put_be32(bytes, kGyzOffset + 0x08, kGyzSize);
  put_be32(bytes, kGyzOffset + 0x0C, 0x20);
  put_be32(bytes, kGyzOffset + 0x20, 0x00011E00);
  put_be32(bytes, kGyzOffset + 0x2C, 3);
  put_be32(bytes, kGyzOffset + 0x30, kRecordTable);
  for (std::uint32_t index = 0; index < 3; ++index) {
    const std::size_t record =
        kGyzOffset + kRecordTable + index * ac6::retail::kRetailTcamRecordBytes;
    put_be32(bytes, record + 0x14, kDataFloor + index * 2);
    put_be32(bytes, record + 0x18, kDataFloor + index * 2 + 1);
  }
  return bytes;
}

std::vector<std::uint8_t> make_scene_triplet(
    std::string_view path,
    std::vector<std::uint8_t> resource = make_tcam_mop()) {
  const std::array<std::string_view, 1> paths{path};
  std::vector<std::uint8_t> resources =
      make_fhm({FhmSlot{std::move(resource)}});
  return make_fhm({FhmSlot{{}, 0xFFFFFFFFu}, FhmSlot{make_nfic_cut()},
                   FhmSlot{std::move(resources)}, FhmSlot{make_scene(paths)}});
}

std::size_t fhm_child_offset(std::span<const std::uint8_t> bytes,
                             std::uint32_t index) {
  const std::optional<ac6::retail::RetailFhmView> fhm =
      ac6::retail::RetailFhmView::open(bytes);
  REQUIRE(fhm.has_value());
  const std::optional<std::span<const std::uint8_t>> child = fhm->child(index);
  REQUIRE(child.has_value());
  return static_cast<std::size_t>(child->data() - bytes.data());
}

void check_mop_view() {
  using ac6::retail::RetailTcamMopView;
  const std::vector<std::uint8_t> bytes = make_tcam_mop();
  const std::optional<RetailTcamMopView> view = RetailTcamMopView::open(bytes);
  REQUIRE(view.has_value());
  REQUIRE(view->bytes().size() == bytes.size());
  REQUIRE(view->gyz_offset() == 0x40);
  REQUIRE(view->gyz_bytes().size() == 0x100);
  REQUIRE(view->record_bytes(0).has_value());
  REQUIRE(view->record_bytes(0)->size() == ac6::retail::kRetailTcamRecordBytes);
  REQUIRE(!view->record_bytes(3).has_value());
  REQUIRE(view->record_data_offsets(2).has_value());
  REQUIRE((*view->record_data_offsets(2))[0] == 0xE4);
  REQUIRE((*view->record_data_offsets(2))[1] == 0xE5);
  REQUIRE(!view->record_data_offsets(3).has_value());

  std::vector<std::uint8_t> opaque_record = bytes;
  opaque_record[0x40 + 0x50 + 0x10] = 0xFF;
  REQUIRE(RetailTcamMopView::open(opaque_record).has_value());

  auto rejected = [&bytes](std::size_t offset, std::uint32_t value) {
    std::vector<std::uint8_t> invalid = bytes;
    put_be32(invalid, offset, value);
    REQUIRE(!RetailTcamMopView::open(invalid).has_value());
  };
  rejected(0x20, 0xFF);                 // outer size
  rejected(0x30, 0x30);                 // wrapper GYZ offset
  rejected(0x40, 0);                    // GYZ magic
  rejected(0x40 + 0x08, 0xFF);          // inner size
  rejected(0x40 + 0x0C, 0x24);          // header size
  rejected(0x40 + 0x20, 0x00011E01);    // content tag
  rejected(0x40 + 0x2C, 2);             // record count
  rejected(0x40 + 0x30, 0x40);          // record table floor
  rejected(0x40 + 0x50 + 0x14, 0xDF);   // data below records
  rejected(0x40 + 0x50 + 0x18, 0x100);  // data at end
}

void check_synthetic_catalog() {
  using ac6::retail::RetailSceneTcamCatalog;
  constexpr std::string_view kPath = "Scene/dd01_01a/dd01_01a_01/Tcam__c01.mop";
  const std::vector<std::uint8_t> payload = make_scene_triplet(kPath);
  const std::optional<RetailSceneTcamCatalog> catalog =
      RetailSceneTcamCatalog::scan(payload);
  REQUIRE(catalog.has_value());
  REQUIRE(catalog->scene_table_count() == 1);
  REQUIRE(catalog->scene_path_count() == 1);
  REQUIRE(catalog->size() == 1);
  REQUIRE(!catalog->empty());
  REQUIRE(catalog->find_exact(kPath) == std::optional<std::size_t>{0});
  REQUIRE(!catalog->find_exact("Scene/missing/Tcam.mop").has_value());
  REQUIRE(catalog->resource(0) != nullptr);
  REQUIRE(catalog->resource(1) == nullptr);
  REQUIRE(catalog->resource(0)->path == kPath);
  REQUIRE(catalog->resource(0)->fhm_path == std::vector<std::uint32_t>({2, 0}));
  REQUIRE(catalog->resource(0)->size == make_tcam_mop().size());
  const std::span<const std::uint8_t> resource =
      std::span<const std::uint8_t>(payload).subspan(
          catalog->resource(0)->payload_offset, catalog->resource(0)->size);
  REQUIRE(ac6::sha256_bytes(resource) == catalog->resource(0)->sha256);

  // The zero-size root slot has an intentionally wild offset. It remains
  // opaque, while changing it again cannot affect the accepted catalogue.
  std::vector<std::uint8_t> opaque_zero = payload;
  put_be32(opaque_zero, 0x14, 0x12345678);
  REQUIRE(RetailSceneTcamCatalog::scan(opaque_zero).has_value());

  std::vector<std::uint8_t> invalid = payload;
  invalid[0] = 'X';
  REQUIRE(!RetailSceneTcamCatalog::scan(invalid).has_value());

  const std::size_t state_offset = fhm_child_offset(payload, 1);
  invalid = payload;
  invalid[state_offset] = 'X';
  REQUIRE(!RetailSceneTcamCatalog::scan(invalid).has_value());

  const std::size_t scene_offset = fhm_child_offset(payload, 3);
  invalid = payload;
  invalid[scene_offset + 6] = 0xFF;
  REQUIRE(!RetailSceneTcamCatalog::scan(invalid).has_value());
  invalid = payload;
  std::fill(invalid.begin() + static_cast<std::ptrdiff_t>(scene_offset),
            invalid.begin() +
                static_cast<std::ptrdiff_t>(
                    scene_offset + ac6::retail::kRetailScenePathRecordBytes),
            static_cast<std::uint8_t>('A'));
  std::copy(std::string_view("Scene/").begin(),
            std::string_view("Scene/").end(),
            invalid.begin() + static_cast<std::ptrdiff_t>(scene_offset));
  REQUIRE(!RetailSceneTcamCatalog::scan(invalid).has_value());
  invalid = payload;
  invalid[scene_offset + kPath.size() + 1] = 'X';
  REQUIRE(!RetailSceneTcamCatalog::scan(invalid).has_value());

  const std::array<std::string_view, 1> parent_path{"Scene/../Tcam__c01.mop"};
  std::vector<std::uint8_t> parent_scene = make_scene(parent_path);
  invalid = payload;
  std::copy(parent_scene.begin(), parent_scene.end(),
            invalid.begin() + static_cast<std::ptrdiff_t>(scene_offset));
  REQUIRE(!RetailSceneTcamCatalog::scan(invalid).has_value());

  invalid = payload;
  // Root child 3 is the Scen extent; it may not be a partial record.
  put_be32(invalid, 0x14 + 4 * 4 + 3 * 4,
           ac6::retail::kRetailScenePathRecordBytes - 1);
  REQUIRE(!RetailSceneTcamCatalog::scan(invalid).has_value());

  const std::size_t resource_offset = catalog->resource(0)->payload_offset;
  invalid = payload;
  put_be32(invalid, resource_offset + 0x40 + 0x20, 0x00011E01);
  REQUIRE(!RetailSceneTcamCatalog::scan(invalid).has_value());

  const std::array<std::string_view, 1> one_path{kPath};
  std::vector<std::uint8_t> empty_resources =
      make_fhm({FhmSlot{{}, 0xDEADBEEFu}});
  invalid =
      make_fhm({FhmSlot{make_nfic_cut()}, FhmSlot{std::move(empty_resources)},
                FhmSlot{make_scene(one_path)}});
  REQUIRE(!RetailSceneTcamCatalog::scan(invalid).has_value());

  const std::array<std::string_view, 2> two_paths{
      kPath, "Scene/dd01_01a/dd01_01a_01/Other.mop"};
  std::vector<std::uint8_t> one_resource = make_fhm({FhmSlot{make_tcam_mop()}});
  invalid =
      make_fhm({FhmSlot{make_nfic_cut()}, FhmSlot{std::move(one_resource)},
                FhmSlot{make_scene(two_paths)}});
  REQUIRE(!RetailSceneTcamCatalog::scan(invalid).has_value());

  // Two individually valid triplets cannot publish the same exact Tcam path.
  std::vector<std::uint8_t> resources_a = make_fhm({FhmSlot{make_tcam_mop()}});
  std::vector<std::uint8_t> resources_b = make_fhm({FhmSlot{make_tcam_mop()}});
  invalid = make_fhm({FhmSlot{make_nfic_cut()}, FhmSlot{std::move(resources_a)},
                      FhmSlot{make_scene(one_path)}, FhmSlot{make_nfic_cut()},
                      FhmSlot{std::move(resources_b)},
                      FhmSlot{make_scene(one_path)}});
  REQUIRE(!RetailSceneTcamCatalog::scan(invalid).has_value());

  // Depth 64 is accepted; the next nested FHM is rejected before descent.
  std::vector<std::uint8_t> deep =
      make_fhm({FhmSlot{std::vector<std::uint8_t>{'X'}}});
  for (std::size_t depth = 0; depth < 64; ++depth) {
    deep = make_fhm({FhmSlot{std::move(deep)}});
  }
  REQUIRE(RetailSceneTcamCatalog::scan(deep).has_value());
  deep = make_fhm({FhmSlot{std::move(deep)}});
  REQUIRE(!RetailSceneTcamCatalog::scan(deep).has_value());

  // Fifty branches of 2,000 nested leaf FHMs cross the independent 100,000
  // container ceiling at depth two, without relying on the depth guard.
  const std::vector<std::uint8_t> leaf =
      make_fhm({FhmSlot{std::vector<std::uint8_t>{'X'}}});
  std::vector<FhmSlot> root_slots;
  root_slots.reserve(50);
  for (std::size_t branch_index = 0; branch_index < 50; ++branch_index) {
    std::vector<FhmSlot> leaf_slots;
    leaf_slots.reserve(2000);
    for (std::size_t leaf_index = 0; leaf_index < 2000; ++leaf_index) {
      leaf_slots.push_back(FhmSlot{leaf});
    }
    root_slots.push_back(FhmSlot{make_fhm(std::move(leaf_slots))});
  }
  const std::vector<std::uint8_t> too_many_containers =
      make_fhm(std::move(root_slots));
  REQUIRE(!RetailSceneTcamCatalog::scan(too_many_containers).has_value());
}

int check_qualified_cache() {
  const char* cache_root = std::getenv("AC6_RETAIL_CACHE");
  if (cache_root == nullptr || *cache_root == '\0') return 77;

  ac6::RetailContentStore store;
  REQUIRE(store.open(cache_root));
  REQUIRE(ac6::sha256_hex(store.index_sha256()) ==
          "cfca517e3f843169ca01fc52700472e66b86365621a922fc27a64a21ab713f85");

  constexpr std::array<std::size_t, 15> kSceneTables{44, 0, 0, 0, 0,  0, 24, 0,
                                                     16, 0, 0, 0, 30, 0, 62};
  constexpr std::array<std::size_t, 15> kScenePaths{
      553, 0, 0, 0, 0, 0, 290, 0, 662, 0, 0, 0, 363, 0, 1082};
  constexpr std::array<std::size_t, 15> kTcams{22, 0, 0, 0, 0,  0, 12, 0,
                                               8,  0, 0, 0, 15, 0, 31};
  constexpr std::string_view kMission01Path =
      "Scene/dd01_01a/dd01_01a_01/Tcam__c01.mop";
  constexpr std::string_view kMission01TcamSha256 =
      "2af69c5ebdf322c473b7aa4599882dc2d5b915a4433a9e58f0ea6ea3340cf2d1";

  std::size_t total_tables = 0;
  std::size_t total_paths = 0;
  std::size_t total_tcams = 0;
  for (std::uint32_t mission_id = 1; mission_id <= 15; ++mission_id) {
    std::optional<ac6::retail::RetailCampaignBundle> bundle =
        ac6::retail::RetailCampaignBundle::open(store, mission_id);
    REQUIRE(bundle.has_value());
    REQUIRE(bundle->mission_id() == mission_id);
    REQUIRE(bundle->data_table_entry() == mission_id + 8);
    const ac6::retail::RetailSceneTcamCatalog& catalog = bundle->scene_tcams();
    REQUIRE(catalog.scene_table_count() == kSceneTables[mission_id - 1]);
    REQUIRE(catalog.scene_path_count() == kScenePaths[mission_id - 1]);
    REQUIRE(catalog.size() == kTcams[mission_id - 1]);
    total_tables += catalog.scene_table_count();
    total_paths += catalog.scene_path_count();
    total_tcams += catalog.size();

    for (std::size_t index = 0; index < catalog.size(); ++index) {
      const ac6::retail::RetailSceneTcamResource* resource =
          catalog.resource(index);
      REQUIRE(resource != nullptr);
      REQUIRE(catalog.find_exact(resource->path) ==
              std::optional<std::size_t>{index});
      const std::optional<std::span<const std::uint8_t>> bytes =
          bundle->tcam_bytes(index);
      REQUIRE(bytes.has_value());
      REQUIRE(bytes->size() == resource->size);
      REQUIRE(ac6::sha256_bytes(*bytes) == resource->sha256);
      REQUIRE(ac6::retail::RetailTcamMopView::open(*bytes).has_value());
    }
    REQUIRE(!bundle->tcam_bytes(catalog.size()).has_value());

    if (mission_id == 1) {
      REQUIRE(bundle->payload().size() == 42446032);
      const ac6::RetailContentRecord* record = store.find(9);
      REQUIRE(record != nullptr);
      REQUIRE(
          ac6::sha256_hex(record->payload_sha256) ==
          "cd81e02189516cb5ba0c08d41659a90ae927fe2eccdad53cf5216db44b6d7a05");
      const std::optional<std::size_t> exact =
          catalog.find_exact(kMission01Path);
      REQUIRE(exact.has_value());
      const ac6::retail::RetailSceneTcamResource* resource =
          catalog.resource(*exact);
      REQUIRE(resource != nullptr);
      REQUIRE(resource->size == 4656);
      REQUIRE(ac6::sha256_hex(resource->sha256) == kMission01TcamSha256);
      REQUIRE(resource->fhm_path ==
              std::vector<std::uint32_t>({22, 1, 0, 1, 0}));

      const ac6::retail::RetailTcamMopView* tcam = nullptr;
      const auto tcam_view =
          ac6::retail::RetailTcamMopView::open(*bundle->tcam_bytes(*exact));
      REQUIRE(tcam_view.has_value());
      tcam = &*tcam_view;

      const auto camera_track =
          ac6::retail::RetailTcamCameraTrack::open(*tcam);
      REQUIRE(camera_track.has_value());
      REQUIRE(camera_track->sample_count() == 121);
      REQUIRE(camera_track->first_frame() == 0);
      REQUIRE(camera_track->last_frame() == 120);
      const auto first_sample = camera_track->sample(1);
      REQUIRE(first_sample.has_value());
      REQUIRE(std::fabs(first_sample->position[0] - (-15824.801758F)) <
              0.001F);
      REQUIRE(std::fabs(first_sample->position[1] - 3284.871826F) < 0.001F);
      REQUIRE(std::fabs(first_sample->position[2] - (-1023.996643F)) <
              0.001F);
      REQUIRE(std::fabs(first_sample->orientation[0] - (-1.284656F)) <
              0.001F);
      REQUIRE(std::fabs(first_sample->orientation[1] - 0.284061F) < 0.001F);
      REQUIRE(std::fabs(first_sample->orientation[2] - 0.000019F) < 0.001F);
      REQUIRE(std::fabs(first_sample->vertical_fov_radians - 0.761F) <
              0.001F);
      const auto last_sample = camera_track->sample(121);
      REQUIRE(last_sample.has_value());
      REQUIRE(last_sample->position != first_sample->position);
      REQUIRE(camera_track->sample(0)->position != first_sample->position);
      REQUIRE(camera_track->sample(122)->position == last_sample->position);

      const std::span<const std::uint8_t> qualified_bytes = *bundle->tcam_bytes(*exact);
      const auto position_offsets = tcam->record_data_offsets(0);
      const auto orientation_offsets = tcam->record_data_offsets(1);
      const auto fov_offsets = tcam->record_data_offsets(2);
      REQUIRE(position_offsets.has_value() && orientation_offsets.has_value() &&
              fov_offsets.has_value());
      std::vector<std::uint8_t> malformed(qualified_bytes.begin(),
                                          qualified_bytes.end());
      put_be32(malformed, tcam->gyz_offset() + 0x50 + 0x10, 120);
      REQUIRE(!ac6::retail::RetailTcamCameraTrack::open(
                    *ac6::retail::RetailTcamMopView::open(malformed))
                   .has_value());
      malformed.assign(qualified_bytes.begin(), qualified_bytes.end());
      put_be32(malformed, tcam->gyz_offset() + (*orientation_offsets)[0],
               0x7FC00000u);
      const auto malformed_view =
          ac6::retail::RetailTcamMopView::open(malformed);
      REQUIRE(malformed_view.has_value());
      REQUIRE(!ac6::retail::RetailTcamCameraTrack::open(*malformed_view)
                   .has_value());
      malformed.assign(qualified_bytes.begin(), qualified_bytes.end());
      const std::size_t position_times_offset =
          tcam->gyz_offset() + (*position_offsets)[1];
      const std::uint16_t first_time =
          read_be16(qualified_bytes, position_times_offset);
      put_be16(malformed, position_times_offset + 2, first_time);
      const auto non_monotonic_view =
          ac6::retail::RetailTcamMopView::open(malformed);
      REQUIRE(non_monotonic_view.has_value());
      REQUIRE(!ac6::retail::RetailTcamCameraTrack::open(*non_monotonic_view)
                   .has_value());
      malformed.assign(qualified_bytes.begin(), qualified_bytes.end());
      put_be32(malformed, tcam->gyz_offset() + (*fov_offsets)[0], 0u);
      const auto bad_fov_view = ac6::retail::RetailTcamMopView::open(malformed);
      REQUIRE(bad_fov_view.has_value());
      REQUIRE(!ac6::retail::RetailTcamCameraTrack::open(*bad_fov_view)
                   .has_value());

      const auto nfic_bytes = bundle->nfic_cut_bytes(*exact);
      REQUIRE(nfic_bytes.has_value());
      REQUIRE(nfic_bytes->size() == 39352);
      const auto nfic = ac6::retail::RetailNficCutView::open(*nfic_bytes);
      REQUIRE(nfic.has_value());
      REQUIRE(nfic->chunk_count() == 9);
      REQUIRE(nfic->event_count() == 2402);
      REQUIRE(nfic->has_terminal_event());
      REQUIRE(nfic->has_dictionary_symbol(0x1001, "MoveCamera"));
      REQUIRE(nfic->has_dictionary_symbol(0x8001, "CutStart"));
      REQUIRE(nfic->has_dictionary_symbol(0x8002, "FrameStart"));
      REQUIRE(nfic->has_dictionary_symbol(0x8003, "FrameTerminate"));
      REQUIRE(nfic->has_dictionary_symbol(0x8004, "CutTerminate"));
      const auto nfic_event0 = nfic->event(0);
      const auto nfic_event1 = nfic->event(1);
      const auto nfic_event2 = nfic->event(2);
      REQUIRE(nfic_event0.has_value() && nfic_event1.has_value() &&
              nfic_event2.has_value());
      REQUIRE(nfic_event0->tag == 0x8001 && nfic_event0->payload.empty());
      REQUIRE(nfic_event1->tag == 0x8002 && nfic_event1->payload.size() == 4 &&
              nfic_event1->payload[3] == 1);
      REQUIRE(nfic_event2->tag == 0x1001 && nfic_event2->payload.size() == 8);
      const auto camera_command = nfic->initial_camera_command();
      REQUIRE(camera_command.has_value());
      REQUIRE(camera_command->scene_object_id == 1);
      REQUIRE(camera_command->tcam_frame == 1);

      std::vector<std::uint8_t> malformed_nfic(nfic_bytes->begin(),
                                               nfic_bytes->end());
      put_be32(malformed_nfic, 16 + 4, 0xFFFFFFFFu);
      REQUIRE(!ac6::retail::RetailNficCutView::open(malformed_nfic)
                   .has_value());
      malformed_nfic.assign(nfic_bytes->begin(), nfic_bytes->end());
      put_be16(malformed_nfic, 16 + 2, 1);
      REQUIRE(!ac6::retail::RetailNficCutView::open(malformed_nfic)
                   .has_value());

      // Offsets, not source spans, survive moving the byte-owning bundle.
      ac6::retail::RetailCampaignBundle moved = std::move(*bundle);
      const std::optional<std::size_t> moved_exact =
          moved.scene_tcams().find_exact(kMission01Path);
      REQUIRE(moved_exact.has_value());
      const std::optional<std::span<const std::uint8_t>> moved_bytes =
          moved.tcam_bytes(*moved_exact);
      REQUIRE(moved_bytes.has_value());
      REQUIRE(moved_bytes->size() == 4656);
      REQUIRE(ac6::sha256_hex(ac6::sha256_bytes(*moved_bytes)) ==
              kMission01TcamSha256);
      const auto moved_nfic = moved.nfic_cut_bytes(*moved_exact);
      REQUIRE(moved_nfic.has_value());
      REQUIRE(moved_nfic->size() == 39352);
      REQUIRE(ac6::retail::RetailNficCutView::open(*moved_nfic).has_value());
    }
  }

  REQUIRE(total_tables == 176);
  REQUIRE(total_paths == 2950);
  REQUIRE(total_tcams == 88);
  std::printf(
      "retail_scene_tcam_cache=pass missions=15 tables=%zu paths=%zu "
      "tcam=%zu\n",
      total_tables, total_paths, total_tcams);
  return 0;
}

}  // namespace

int main() {
  check_mop_view();
  check_synthetic_catalog();
  const int retail_status = check_qualified_cache();
  if (retail_status == 77) {
    std::printf("retail_scene_tcam_synthetic=pass cache=skip\n");
    return 77;
  }
  return retail_status;
}

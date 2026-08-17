#include "retail_session_store_tests.h"

#include "ac6/retail_campaign_bundle.h"
#include "test_fixtures.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <span>
#include <string>
#include <unistd.h>

namespace {

constexpr std::uint32_t kMissionId = 1;
constexpr float kFixedDt = 1.0F / 60.0F;
using ac6::retail::RetailSession;

class TempStoreRoot final {
 public:
  TempStoreRoot() {
    static std::atomic<unsigned> next{};
    path_ = std::filesystem::temp_directory_path() /
            ("ac6-retail-session-store-" + std::to_string(::getpid()) + "-" +
             std::to_string(next++));
    REQUIRE(std::filesystem::create_directories(path_));
  }
  ~TempStoreRoot() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }
  const std::filesystem::path& path() const noexcept { return path_; }

 private:
  std::filesystem::path path_;
};

void write_file(const std::filesystem::path& path,
                std::span<const std::uint8_t> bytes) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  REQUIRE(static_cast<bool>(output));
  output.write(reinterpret_cast<const char*>(bytes.data()),
               static_cast<std::streamsize>(bytes.size()));
  REQUIRE(static_cast<bool>(output));
}

void put_be32(std::vector<std::uint8_t>& bytes, std::size_t offset,
              std::uint32_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value >> 24U);
  bytes[offset + 1] = static_cast<std::uint8_t>(value >> 16U);
  bytes[offset + 2] = static_cast<std::uint8_t>(value >> 8U);
  bytes[offset + 3] = static_cast<std::uint8_t>(value);
}

void put_be16(std::vector<std::uint8_t>& bytes, std::size_t offset,
              std::uint16_t value) {
  bytes[offset] = static_cast<std::uint8_t>(value >> 8U);
  bytes[offset + 1] = static_cast<std::uint8_t>(value);
}

ac6::RetailIdentityPolicy make_store_source(
    const std::filesystem::path& source,
    const std::vector<std::uint8_t>& payload, bool invalid_child_range = false) {
  REQUIRE(payload.size() <= UINT32_MAX - 48U);
  REQUIRE(std::filesystem::create_directories(source));
  const std::vector<std::uint8_t> xex{'X', 'E', 'X', '2'};
  const std::vector<std::uint8_t> data01{'P'};
  std::vector<std::uint8_t> campaign_payload(48U + payload.size(), 0);
  const std::array<std::uint8_t, 4> fhm{'F', 'H', 'M', ' '};
  std::copy(fhm.begin(), fhm.end(), campaign_payload.begin());
  campaign_payload[4] = 1;
  campaign_payload[5] = 1;
  put_be16(campaign_payload, 6, 0x10);
  put_be32(campaign_payload, 0x10, 1);
  put_be32(campaign_payload, 0x14,
           invalid_child_range ? UINT32_MAX - 15U : 48U);
  put_be32(campaign_payload, 0x18,
           static_cast<std::uint32_t>(payload.size()));
  std::copy(payload.begin(), payload.end(), campaign_payload.begin() + 48);

  std::vector<std::uint8_t> data00(9U + campaign_payload.size());
  for (std::size_t index = 0; index < 9; ++index) {
    data00[index] = static_cast<std::uint8_t>(index);
  }
  std::copy(campaign_payload.begin(), campaign_payload.end(), data00.begin() + 9);
  ac6::retail_mode1_xor(std::span<std::uint8_t>(data00).subspan(9), 9);

  std::vector<std::uint8_t> table(8U + 10U * 16U, 0);
  put_be32(table, 0, 10);
  put_be32(table, 4, 2);
  for (std::uint32_t index = 0; index < 9; ++index) {
    const std::size_t row = 8U + index * 16U;
    put_be32(table, row, 0x00020000U);
    put_be32(table, row + 4, index);
    put_be32(table, row + 8, 1);
    put_be32(table, row + 12, 1);
  }
  const std::size_t mission_row = 8U + 9U * 16U;
  put_be32(table, mission_row, 0x00020000U);
  put_be32(table, mission_row + 4, 9);
  put_be32(table, mission_row + 8,
           static_cast<std::uint32_t>(campaign_payload.size()));
  put_be32(table, mission_row + 12,
           static_cast<std::uint32_t>(campaign_payload.size()));

  write_file(source / "default.xex", xex);
  write_file(source / "DATA.TBL", table);
  write_file(source / "DATA00.PAC", data00);
  write_file(source / "DATA01.PAC", data01);

  ac6::RetailIdentityPolicy policy;
  policy.data_table_entries = 10;
  policy.pack_count = 2;
  policy.identity.xex_size = xex.size();
  policy.identity.data_table_size = table.size();
  policy.identity.data00_size = data00.size();
  policy.identity.data01_size = data01.size();
  REQUIRE(ac6::sha256_file(source / "default.xex", policy.identity.xex_sha256));
  REQUIRE(ac6::sha256_file(source / "DATA.TBL", policy.identity.data_table_sha256));
  REQUIRE(ac6::sha256_file(source / "DATA00.PAC", policy.identity.data00_sha256));
  REQUIRE(ac6::sha256_file(source / "DATA01.PAC", policy.identity.data01_sha256));
  return policy;
}

}  // namespace

void check_store_backed_session(const std::vector<std::uint8_t>& payload,
                                RetailSessionInputFn input,
                                RetailSessionFrameHashFn hash_frame) {
  TempStoreRoot root;
  const std::filesystem::path source = root.path() / "source";
  const std::filesystem::path cache = root.path() / "cache";
  const ac6::RetailIdentityPolicy policy = make_store_source(source, payload);
  const std::array<std::uint32_t, 1> selected{9};
  const ac6::RetailImportReport imported =
      ac6::RetailContentImporter(policy).run(source, cache, selected);
  REQUIRE(imported.passed());
  REQUIRE(imported.imported_records == 1);

  ac6::RetailContentStore store(policy);
  REQUIRE(store.open(cache));
  const std::optional<ac6::retail::RetailCampaignBundle> bundle =
      ac6::retail::RetailCampaignBundle::open(store, kMissionId);
  REQUIRE(bundle.has_value());
  REQUIRE(bundle->child_count() == 1);
  REQUIRE(bundle->child(0).has_value());
  REQUIRE(bundle->child(0)->size() == payload.size());
  const ac6::CampaignLoadout loadout{1, 1, true};
  const ac6::retail::RetailMissionBundleConfig mission_config{
      kMissionId, ac6::retail::RetailDifficulty::Ace, loadout};
  const std::optional<ac6::retail::RetailMissionBundle> mission =
      ac6::retail::RetailMissionBundle::open(store, mission_config);
  REQUIRE(mission.has_value());
  REQUIRE(mission->difficulty() == ac6::retail::RetailDifficulty::Ace);
  REQUIRE(mission->loadout() == loadout);
  REQUIRE(!ac6::retail::RetailMissionBundle::open(
               store, {kMissionId, static_cast<ac6::retail::RetailDifficulty>(5),
                       loadout})
               .has_value());
  std::unique_ptr<RetailSession> session =
      RetailSession::open(store, loadout,
                          {kMissionId, {0, 0},
                           ac6::retail::kRetailOpeningCameraModeWord,
                           ac6::retail::RetailDifficulty::Ace});
  REQUIRE(session != nullptr);
  REQUIRE(session->bundle().has_value());
  REQUIRE(session->bundle()->data_table_entry == 9);
  REQUIRE(session->bundle()->loadout == loadout);
  REQUIRE(session->bundle()->difficulty == ac6::retail::RetailDifficulty::Ace);
  REQUIRE(session->bundle()->content_index_sha256 == store.index_sha256());
  // The frontend loadout id is provenance, not a qualified WeaponBin
  // definition. Store-backed sessions expose no synthetic combat profile.
  REQUIRE(session->execution().primary_weapon_id() == 0);
  REQUIRE(session->execution().weapon_count() == 0);
  REQUIRE(!session->fire_primary());
  REQUIRE(session->campaign() != nullptr);
  REQUIRE(session->campaign()->status(kMissionId) != nullptr);
  REQUIRE(session->campaign()->status(kMissionId)->state ==
          ac6::CampaignMissionState::Active);
  const auto require_external_drive_frame = [](const RetailSession& current,
                                                const auto& frame) {
    REQUIRE(!frame.script_ended);
    REQUIRE(frame.sub_mission == 0);
    REQUIRE(frame.step == 0);
    REQUIRE(current.state() == ac6::ScenarioState::Gameplay);
    REQUIRE(frame.world.mission_ready);
    const ac6::MissionDebrief debrief = current.debrief();
    REQUIRE(debrief.outcome == ac6::MissionOutcome::InProgress);
    REQUIRE(debrief.completed_objectives == 0);
    REQUIRE(debrief.failed_objectives == 0);
  };

  constexpr std::size_t kControlledTicks = 3600;
  REQUIRE(session->scenario().counter_capacity() == 339);
  for (std::uint16_t id = 0; id < session->scenario().counter_capacity(); ++id) {
    REQUIRE(session->counter(id).has_value());
    REQUIRE(*session->counter(id) == 0);
  }
  ac6::retail::RetailSessionFrame primary_frame;
  for (std::size_t tick = 1; tick <= kControlledTicks / 2; ++tick) {
    primary_frame = session->tick(kFixedDt, input(tick));
    require_external_drive_frame(*session, primary_frame);
  }
  ac6::MissionExecution::Checkpoint checkpoint;
  REQUIRE(session->save_checkpoint(checkpoint));
  const ac6::SessionSaveSnapshot snapshot{
      kMissionId, store.index_sha256(), session->execution().snapshot(),
      session->campaign_snapshot(), checkpoint};
  std::unique_ptr<RetailSession> resumed = RetailSession::open(
      store, loadout,
      {kMissionId, {0, 0}, ac6::retail::kRetailOpeningCameraModeWord,
       ac6::retail::RetailDifficulty::Ace});
  REQUIRE(resumed != nullptr);
  REQUIRE(resumed->restore_save(snapshot));
  for (std::size_t tick = kControlledTicks / 2 + 1; tick <= kControlledTicks; ++tick) {
    primary_frame = session->tick(kFixedDt, input(tick));
    const ac6::retail::RetailSessionFrame resumed_frame =
        resumed->tick(kFixedDt, input(tick));
    require_external_drive_frame(*session, primary_frame);
    require_external_drive_frame(*resumed, resumed_frame);
    REQUIRE(hash_frame(primary_frame) == hash_frame(resumed_frame));
  }
  std::unique_ptr<RetailSession> replay = RetailSession::open(
      store, loadout,
      {kMissionId, {0, 0}, ac6::retail::kRetailOpeningCameraModeWord,
       ac6::retail::RetailDifficulty::Ace});
  REQUIRE(replay != nullptr);
  ac6::retail::RetailSessionFrame replay_frame;
  for (std::size_t tick = 1; tick <= kControlledTicks; ++tick) {
    replay_frame = replay->tick(kFixedDt, input(tick));
    require_external_drive_frame(*replay, replay_frame);
  }
  REQUIRE(hash_frame(primary_frame) == hash_frame(replay_frame));
  REQUIRE(primary_frame.world.tick == kControlledTicks);
  REQUIRE(primary_frame.sub_mission == 0);
  REQUIRE(primary_frame.step == 0);
  REQUIRE(!primary_frame.script_ended);
  REQUIRE(session->state() == ac6::ScenarioState::Gameplay);
  REQUIRE(session->debrief().outcome == ac6::MissionOutcome::InProgress);
  REQUIRE(session->debrief().completed_objectives == 0);
  REQUIRE(session->debrief().failed_objectives == 0);
  for (std::uint16_t id = 0; id < session->scenario().counter_capacity(); ++id) {
    REQUIRE(session->counter(id).has_value());
    REQUIRE(*session->counter(id) == 0);
  }

  REQUIRE(RetailSession::open(store, {1, 1, false}, {kMissionId, {0, 0}}) ==
          nullptr);
  REQUIRE(RetailSession::open(store, loadout, {0, {0, 0}}) == nullptr);
  REQUIRE(RetailSession::open(store, loadout, {16, {0, 0}}) == nullptr);
  REQUIRE(RetailSession::open(store, loadout, {kMissionId, {0, 0}, 4}) == nullptr);
  REQUIRE(RetailSession::open(
              store, loadout,
              {kMissionId, {0, 0}, ac6::retail::kRetailOpeningCameraModeWord,
               ac6::retail::RetailDifficulty::Ace,
               ac6::retail::RetailScriptDrive::DiagnosticFixedTick}) == nullptr);
  REQUIRE(RetailSession::open(
              store, loadout,
              {kMissionId, {0, 0}, ac6::retail::kRetailOpeningCameraModeWord,
               ac6::retail::RetailDifficulty::Ace,
               ac6::retail::RetailScriptDrive::QualifiedRuntime}) == nullptr);

  TempStoreRoot invalid_root;
  const std::filesystem::path invalid_source = invalid_root.path() / "source";
  const std::filesystem::path invalid_cache = invalid_root.path() / "cache";
  const ac6::RetailIdentityPolicy invalid_policy =
      make_store_source(invalid_source, payload, true);
  REQUIRE(ac6::RetailContentImporter(invalid_policy)
              .run(invalid_source, invalid_cache, selected)
              .passed());
  ac6::RetailContentStore invalid_store(invalid_policy);
  REQUIRE(invalid_store.open(invalid_cache));
  REQUIRE(!ac6::retail::RetailCampaignBundle::open(invalid_store, kMissionId)
               .has_value());
  REQUIRE(RetailSession::open(invalid_store, loadout,
                              {kMissionId, {0, 0}}) == nullptr);
}

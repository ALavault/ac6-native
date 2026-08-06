#pragma once

#include "ac6/product_runtime.h"

namespace ac6 {

enum class FrontendState : std::uint8_t { Title, NewGame, Briefing, Hangar, Loading, Mission, Debrief };
enum class FrontendDifficulty : std::uint8_t { Normal, Easy, Hard };
enum class FrontendControls : std::uint8_t { Normal, Expert };
enum class FrontendLanguage : std::uint8_t { English, French, German, Italian, Spanish };

struct FrontendSettings {
  FrontendDifficulty difficulty{FrontendDifficulty::Normal};
  FrontendControls controls{FrontendControls::Normal};
  FrontendLanguage language{FrontendLanguage::English};
  bool valid() const noexcept {
    return difficulty == FrontendDifficulty::Normal && controls == FrontendControls::Normal &&
           language == FrontendLanguage::English;
  }
};

class FrontendController final {
 public:
  FrontendState state() const noexcept { return state_; }
  std::uint32_t selected_mission() const noexcept { return selected_mission_; }
  const FrontendSettings& settings() const noexcept { return settings_; }
  bool configure(FrontendSettings settings) noexcept;
  void set_campaign(CampaignProgression* campaign) noexcept { campaign_ = campaign; }
  bool select_mission(const MissionCatalog& catalog, std::uint32_t mission_id) noexcept;
  bool set_loadout(CampaignLoadout loadout) noexcept;
  const MissionDefinition* mission_definition(const MissionCatalog& catalog) const noexcept;
  bool launch_selected(const MissionCatalog& catalog, const MissionLaunchDatabase& launches,
                       MissionExecution& execution) const noexcept;
  bool enter_debrief(const MissionExecution& execution) noexcept;
  bool return_to_campaign() noexcept;
  const MissionDebrief* debrief() const noexcept {
    return debrief_.has_value() ? &*debrief_ : nullptr;
  }
  bool advance() noexcept;
  bool dispatch(Event event) noexcept;
  bool dispatch_buttons(const InputMappingDatabase& mappings,
                        std::uint16_t buttons) noexcept;

 private:
  FrontendState state_{FrontendState::Title};
  std::uint32_t selected_mission_{};
  FrontendSettings settings_{};
  CampaignProgression* campaign_{};
  std::optional<MissionDebrief> debrief_;
};

}  // namespace ac6

// The world-position resolver of 0x822953F0, against the payload's own records.
//
// The interesting claim is not that the port compiles: it is that the mode byte
// the retail branch tests actually separates world coordinates from offsets in
// Mission 01's data. Nobody sorted these records by hand - the byte was read off
// the branch at 0x82295420 and the two populations were measured afterwards.
//
// usage: retail-world-position-tests PAYLOAD [REPORT_JSON]
// exit 77 means the retail payload was absent; it is never committed.

#include "ac6/retail_world_position.h"
#include "test_fixtures.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

using ac6::retail::AnchorFrame;
using ac6::retail::ScenarioPositionRecord;
using ac6::retail::ScenarioVector;

struct Spread {
  float min{}, max{}, median{};
};

Spread spread(std::vector<float> values) {
  REQUIRE(!values.empty());
  std::sort(values.begin(), values.end());
  return {values.front(), values.back(), values[values.size() / 2]};
}

bool near(float a, float b, float tolerance) { return std::fabs(a - b) <= tolerance; }

// The REQUIRE macro takes one argument, and a braced vector carries commas.
bool same(const ScenarioVector& v, float x, float y, float z) {
  return v.x == x && v.y == y && v.z == z;
}

void check_algebra() {
  // Mode 0: the triple is the answer, whatever anchor is offered.
  const ScenarioPositionRecord absolute{0, 1856.0f, 1500.0f, -16416.0f, 0, 0, 255, 255, 0, 255};
  const AnchorFrame anchor{{1000.0f, 200.0f, -500.0f}, 1.0f};
  const std::optional<ScenarioVector> plain =
      ac6::retail::resolve_world_position(absolute, &anchor);
  REQUIRE(plain.has_value());
  REQUIRE(same(*plain, 1856.0f, 1500.0f, -16416.0f));

  // Mode 1 without an anchor is refused rather than answered from the offset.
  ScenarioPositionRecord relative = absolute;
  relative.mode = 1;
  REQUIRE(!ac6::retail::resolve_world_position(relative).has_value());

  // A zero heading leaves the offset alone and only translates it.
  const AnchorFrame straight{{100.0f, 20.0f, -30.0f}, 0.0f};
  relative = {0, 5.0f, 7.0f, 11.0f, 0, 1, 0, 0, 0, 255};
  const std::optional<ScenarioVector> translated =
      ac6::retail::resolve_world_position(relative, &straight);
  REQUIRE(translated.has_value());
  REQUIRE(same(*translated, 105.0f, 27.0f, -19.0f));

  // The sign convention, stated as a test rather than as a comment: a purely
  // forward offset must land along the anchor's own forward direction, which
  // 0x82093808 defines as atan2(forward.x, forward.z).
  const float heading = ac6::retail::anchor_heading(1.0f, 0.0f);  // facing +x
  REQUIRE(near(heading, 1.5707963f, 1.0e-5f));
  const AnchorFrame facing_x{{0.0f, 0.0f, 0.0f}, heading};
  const ScenarioPositionRecord forward{0, 0.0f, 0.0f, 10.0f, 0, 1, 0, 0, 0, 255};
  const std::optional<ScenarioVector> ahead =
      ac6::retail::resolve_world_position(forward, &facing_x);
  REQUIRE(ahead.has_value());
  REQUIRE(near(ahead->x, 10.0f, 1.0e-3f));
  REQUIRE(near(ahead->z, 0.0f, 1.0e-3f));

  // Both degenerate components give the constant branch of 0x820936E8, not a
  // call to atan2 on two zeros.
  REQUIRE(ac6::retail::anchor_heading(0.0f, 0.0f) == 0.0f);

  // The height flag needs a height; without one the resolver refuses.
  ScenarioPositionRecord grounded = absolute;
  grounded.flags = ac6::retail::kPositionHeightFromQuery;
  REQUIRE(!ac6::retail::resolve_world_position(grounded).has_value());
  const float ground = 42.0f;
  const std::optional<ScenarioVector> lifted =
      ac6::retail::resolve_world_position(grounded, nullptr, &ground);
  REQUIRE(lifted.has_value());
  REQUIRE(lifted->y == 42.0f + 1500.0f);
  REQUIRE(lifted->x == absolute.x && lifted->z == absolute.z);
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "usage: %s PAYLOAD [REPORT_JSON]\n", argv[0]);
    return 2;
  }
  check_algebra();
  if (!std::filesystem::exists(argv[1])) {
    std::fprintf(stderr, "retail payload absent\n");
    return 77;
  }
  std::ifstream input(argv[1], std::ios::binary);
  std::ostringstream buffer;
  buffer << input.rdbuf();
  const std::string text = buffer.str();
  const std::optional<ac6::retail::ScenarioPayload> payload =
      ac6::retail::ScenarioPayload::open(
          std::vector<std::uint8_t>(text.begin(), text.end()));
  REQUIRE(payload.has_value());
  const std::optional<ac6::retail::MissionScenario> scenario =
      ac6::retail::MissionScenario::parse(*payload);
  REQUIRE(scenario.has_value());

  const std::vector<ScenarioPositionRecord>& records = scenario->positions();
  REQUIRE(records.size() == 890);

  std::vector<float> ax, az, ay, rx, rz, ry;
  std::size_t absolute = 0, anchored = 0, height_flagged = 0, inside = 0;
  for (const ScenarioPositionRecord& record : records) {
    if ((record.flags & ac6::retail::kPositionHeightFromQuery) != 0) height_flagged += 1;
    if (record.mode == 1) {
      anchored += 1;
      rx.push_back(record.x); ry.push_back(record.y); rz.push_back(record.z);
      // Every anchored record must be refused without an anchor, and answered
      // with one. That is the branch, exercised on real data.
      REQUIRE(!ac6::retail::resolve_world_position(record).has_value());
      const AnchorFrame frame{{0.0f, 0.0f, 0.0f}, 0.0f};
      REQUIRE(ac6::retail::resolve_world_position(record, &frame).has_value());
      continue;
    }
    absolute += 1;
    ax.push_back(record.x); ay.push_back(record.y); az.push_back(record.z);
    if (std::fabs(record.x) <= 50000.0f && std::fabs(record.z) <= 50000.0f) inside += 1;
    if ((record.flags & ac6::retail::kPositionHeightFromQuery) != 0) continue;
    const std::optional<ScenarioVector> resolved =
        ac6::retail::resolve_world_position(record);
    REQUIRE(resolved.has_value());
    REQUIRE(same(*resolved, record.x, record.y, record.z));
  }
  REQUIRE(absolute == 811 && anchored == 79);

  const Spread absolute_x = spread(ax), absolute_z = spread(az);
  const Spread relative_x = spread(rx), relative_y = spread(ry), relative_z = spread(rz);

  // The control. The mode byte came off a branch; these two populations were
  // measured after the fact, and they do not overlap in character: the mode-0
  // records reach tens of thousands of world units on both horizontal axes,
  // while the mode-1 records sit at a median of exactly zero on all three -
  // which is what an offset in someone else's frame looks like.
  REQUIRE(absolute_x.min < -50000.0f && absolute_x.max > 50000.0f);
  REQUIRE(absolute_z.min < -50000.0f && absolute_z.max > 50000.0f);
  REQUIRE(relative_x.median == 0.0f && relative_y.median == 0.0f &&
          relative_z.median == 0.0f);
  REQUIRE(std::max(std::fabs(relative_x.min), std::fabs(relative_x.max)) < 5000.0f);
  REQUIRE(std::max(std::fabs(relative_z.min), std::fabs(relative_z.max)) < 20000.0f);
  // And the world coordinates are mostly inside the rectangle the sub-mission
  // script installs - mostly, not entirely: 53 of them lie outside it, so this
  // is reported and not asserted into a rule.
  REQUIRE(inside == 758);

  std::printf("retail_world_position records=%zu absolute=%zu anchored=%zu "
              "height_flagged=%zu inside_area=%zu\n",
              records.size(), absolute, anchored, height_flagged, inside);

  if (argc >= 3) {
    std::ofstream report(argv[2]);
    REQUIRE(static_cast<bool>(report));
    report << "{\n"
           << "  \"schema\": \"ac6.retail-world-position.v1\",\n"
           << "  \"mission_id\": 1,\n"
           << "  \"source\": \"retail scenario container only, no manifest\",\n"
           << "  \"retail_function\": \"0x822953F0\",\n"
           << "  \"call_site\": \"0x82295BF0\",\n"
           << "  \"position_records\": " << records.size() << ",\n"
           << "  \"absolute_mode_0\": " << absolute << ",\n"
           << "  \"anchored_mode_1\": " << anchored << ",\n"
           << "  \"height_flagged\": " << height_flagged << ",\n"
           << "  \"absolute_x\": [" << absolute_x.min << ", " << absolute_x.max << "],\n"
           << "  \"absolute_z\": [" << absolute_z.min << ", " << absolute_z.max << "],\n"
           << "  \"anchored_median\": [" << relative_x.median << ", " << relative_y.median
           << ", " << relative_z.median << "],\n"
           << "  \"absolute_inside_submission_0_rectangle\": " << inside << ",\n"
           << "  \"spawn_positions_still_open\": true\n"
           << "}\n";
    REQUIRE(static_cast<bool>(report));
  }
  return 0;
}

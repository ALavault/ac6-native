// The contracted flight integrator, flown across the map's real terrain.
//
// THE QUESTION, from cycle 1467's "not established": do `at68` and the terrain
// share an origin? `kMidFloor` is 10.0 at 0x82003214 and retail applies it to
// `at68` and to nothing else -- a CONSTANT floor. This map's ground runs
// 0.00 to 487.44.
//
// A constant floor cannot keep an aircraft above terrain that rises. So either
// the integrator is not what keeps it there, or the two do not share an origin.
// This flies the contracted integrator over the contracted heightfield and
// measures how often the aircraft ends a tick below the ground under it. The
// answer distinguishes nothing about the ORIGIN -- but it does establish, as a
// number rather than an argument, that the integrator alone is insufficient.
//
// DATA DRIVEN, exiting 77 when the map container is absent.
#include "ac6/retail_flight_session.h"
#include "ac6/retail_flight_step.h"
#include "ac6/retail_terrain_field.h"
#include "ac6/retail_transform.h"

#include <cmath>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {
int failures = 0;
void check(bool c, const char* w) {
  if (!c) { std::printf("FAIL  %s\n", w); ++failures; }
}
std::vector<std::uint8_t> slurp(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
}
}  // namespace

int main(int argc, char** argv) {
  using namespace ac6::retail;
  check(kMidFloor == 10.0F, "the floor is the constant 10.0 of 0x82003214");

  if (argc < 2) { std::fprintf(stderr, "usage: tests MAP_FHM_DIR\n"); return 77; }
  const std::filesystem::path dir = argv[1];
  if (!std::filesystem::exists(dir / "005_Bl_02_b8.bin")) {
    std::fprintf(stderr, "no map container — skipping\n");
    return failures == 0 ? 77 : 1;
  }
  const auto grid = slurp(dir / "004_00_01_02_03.bin");
  const auto patches = slurp(dir / "005_Bl_02_b8.bin");
  const auto field =
      TerrainField::open(grid.data(), grid.size(), patches.data(), patches.size());
  check(field.has_value(), "the heightfield opens");
  if (!field) return 1;

  // A level run from the open sea, north across the coast and the city, into
  // the hills. The rates are retail's units and the scale is the contracted
  // kRateToStep; the heading and the altitude are mine.
  //
  // The first version of this flew inland from (-40000, -40000) and was below
  // ground for all 1800 ticks -- true, and useless: that line never leaves
  // high ground, so it could not show the crossing. This one starts over water.
  FlightSessionState state{};
  state.position.at64 = -1500.0F;
  state.position.at68 = 60.0F;
  state.position.at72 = 6000.0F;

  // RATES ARE A DIRECTION AND rate_scale IS THE SPEED. retail_flight_session.h
  // says so, from cycle 1415: the three rates come from a vector normalise the
  // campaign refuses to approximate, and `rate_scale` is [model+32], clamped
  // against [model+1264]. Cycle 1468 passed a magnitude-5400 "direction" and
  // kRateToStep as the "scale", which is neither, and then recorded "what
  // rate_scale is for" as not established -- in a header that explains it.
  FlightRates rates{};
  rates.to64 = 0.0F;
  rates.to68 = 0.0F;
  rates.to72 = -1.0F;                   // north: a unit direction

  // 1500 in retail's units, which kRateToStep divides by 3.6 -- so about 417
  // world units a second, and 6.94 a tick at 60 Hz. 6000 ticks is 100 seconds
  // and covers 41,600 units: sea, coast, city and hills.
  const float rate_scale = 1500.0F;

  std::size_t ticks = 0, below = 0, off_map = 0, above = 0;
  float lowest_clearance = 1e30F, highest_ground = -1e30F;
  for (int i = 0; i < 6000; ++i) {      // 100 seconds at 60 Hz
    integrate_session_position(state, rates, rate_scale, 0.0F, 1.0F / 60.0F);
    float ground = 0.0F;
    if (!field->height_at(state.position.at64, state.position.at72, &ground)) {
      ++off_map;
      continue;
    }
    ++ticks;
    highest_ground = std::fmax(highest_ground, ground);
    const float clearance = state.position.at68 - ground;
    lowest_clearance = std::fmin(lowest_clearance, clearance);
    if (clearance < 0.0F) ++below; else ++above;
  }

  check(ticks > 5000, "the run stayed on the map");
  check(state.position.at68 >= kMidFloor, "the floor held the vertical");
  check(highest_ground > 50.0F, "the run crossed ground higher than it flew");
  check(above > 100, "the run spent real time clear of the ground");
  check(below > 100,
        "and real time BELOW it -- a constant floor does not follow terrain");

  // AND A HEADING THAT IS NOT MINE. The direction above is a unit vector I
  // chose. `step_flight_session` produces a basis from contracted arithmetic,
  // and row 2 of that basis is a normalised forward -- the same shape the rates
  // want. Feeding it in leaves only the speed invented.
  {
    FlightModelConfig config{};
    config.limits = FlightRotationLimits{5.0F, 1.399999976158142F, 5.400000095367432F};
    config.rates304 = LiveAxisRates{4.0F, 2.0F};
    config.rates308 = LiveAxisRates{3.0F, 1.5F};
    config.rates312 = LiveAxisRates{5.0F, 2.5F};
    config.servo304 = RateServoAxis{0.0F, 0.0F, 2.0F, 3.0F};
    config.servo308 = RateServoAxis{0.0F, 0.0F, 2.0F, 3.0F};
    config.servo312 = RateServoAxis{0.0F, 0.0F, 2.0F, 3.0F};
    config.rampRate952 = 3.0F;
    config.rampRate956 = 3.0F;
    config.rampThreshold404 = 0.5F;

    FlightSessionState flown{};
    flown.position.at64 = -1500.0F;
    flown.position.at68 = 400.0F;
    flown.position.at72 = 6000.0F;
    FlightStick roll{};
    roll.target14 = 0.35F;
    roll.increment14 = 1.0F;

    float worst_row_error = 0.0F;
    std::size_t moved = 0;
    const FlightPosition start = flown.position;
    for (int i = 0; i < 3000; ++i) {
      step_flight_session(flown, config, roll, 1.0F / 60.0F);
      const auto& row = flown.basis.rows[2];
      const float length =
          std::sqrt(row[0] * row[0] + row[1] * row[1] + row[2] * row[2]);
      worst_row_error = std::fmax(worst_row_error, std::fabs(length - 1.0F));
      FlightRates heading{};
      heading.to64 = row[0];
      heading.to68 = row[1];
      heading.to72 = row[2];
      integrate_session_position(flown, heading, 1500.0F, 0.0F, 1.0F / 60.0F);
    }
    const float travelled =
        std::sqrt((flown.position.at64 - start.at64) * (flown.position.at64 - start.at64) +
                  (flown.position.at72 - start.at72) * (flown.position.at72 - start.at72));
    if (flown.position.at64 != start.at64 || flown.position.at72 != start.at72) ++moved;
    check(worst_row_error < 1e-3F,
          "the basis's forward row is a unit vector, which is what the "
          "integrator's direction parameter wants");
    check(moved == 1 && travelled > 1000.0F,
          "and flying along it moves the aircraft");
    std::printf("basis-driven: forward |len-1| <= %.2e  travelled %.0f  "
                "final (%.0f, %.0f, %.0f)\n",
                worst_row_error, travelled, flown.position.at64,
                flown.position.at68, flown.position.at72);
  }

  std::printf("ticks %zu (off map %zu)  at68 %.2f  highest ground %.2f  "
              "lowest clearance %.2f  clear %zu  below %zu\n",
              ticks, off_map, state.position.at68, highest_ground,
              lowest_clearance, above, below);
  if (failures == 0) std::printf("flight over terrain OK\n");
  return failures == 0 ? 0 : 1;
}

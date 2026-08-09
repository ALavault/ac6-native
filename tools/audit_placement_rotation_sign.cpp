// Which sign is the placement rotation? Decided against the water bit, not by eye.
//
// Cycle 1451 established that `tag >> 16` is an angle: the circular resultant of
// four times it is 0.9757 against 0.68-0.70 at every other harmonic, and zero of
// 2000 trials under either null model reached that. What it could NOT establish
// is the sign, because both `+theta` and `-theta` produce a four-fold set, and a
// top-down render of the city three ways did not discriminate either.
//
// The test that can: a building does not stand in the bay. Sample each part's
// footprint, rotate it by `+theta`, `-theta` or not at all, and ask the ported
// water grid -- a different file, decoded from a different retail function --
// how much of it lands on water. The wrong rotation swings footprints out over
// the harbour; the right one does not.
//
// A null model runs alongside: the same footprints at RANDOM angles. If neither
// sign beats the null, the footprint proxy is too blunt to decide and this tool
// says so instead of picking a winner.
//
// usage: audit_placement_rotation_sign MAPDIR [samples_per_axis]
#include "ac6/retail_map_placement.h"
#include "ac6/retail_map_water.h"
#include "ac6/retail_ndxr_container.h"
#include "ac6/retail_ndxr_geometry.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace {
std::vector<std::uint8_t> Read(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                   std::istreambuf_iterator<char>());
}
// The real vertices, subsampled. An axis-aligned box over a whole city block is
// mostly empty space, and the first version of this tool used one: every
// arrangement came out within 0.06 points and the random null TIED +theta. The
// footprint has to be where the geometry actually is.
struct Footprint { std::vector<std::pair<float, float>> points; bool valid; };
constexpr std::size_t kMaxPoints = 400;
std::uint32_t next(std::uint32_t& s) { s = s * 1664525u + 1013904223u; return s; }
}  // namespace

int main(int argc, char** argv) {
  using namespace ac6::retail;
  if (argc < 2) { std::fprintf(stderr, "usage: tool MAPDIR\n"); return 2; }
  const std::string dir = argv[1];

  const auto pdl = Read(dir + "/011_00_00_00_00.bin");
  const auto mca = Read(dir + "/001_MCA_00.bin");
  const auto mci = Read(dir + "/003_MCI_00.bin");
  const auto mcd = Read(dir + "/002_MCD_00.bin");
  const auto placement = MapPlacement::open(pdl.data(), pdl.size());
  const auto water = MapWaterGrid::open(mca.data(), mca.size(), mci.data(),
                                        mci.size(), mcd.data(), mcd.size());
  if (!placement || !water) { std::fprintf(stderr, "refused\n"); return 1; }

  std::map<std::uint16_t, Footprint> parts;
  auto footprint = [&](std::uint16_t id) -> const Footprint& {
    auto it = parts.find(id);
    if (it != parts.end()) return it->second;
    Footprint f{{}, false};
    std::vector<std::pair<float, float>> all;
    char name[64];
    std::snprintf(name, sizeof(name), "/014_FHM/%03u_NDXR.ndxr", id);
    const auto bytes = Read(dir + name);
    if (!bytes.empty()) {
      if (const auto c = NdxrContainer::Open(bytes.data(), bytes.size())) {
        for (std::uint16_t r = 0; r < c->record_count(); ++r) {
          const auto rec = c->Record(r);
          if (!rec) continue;
          for (std::uint16_t k = 0; k < rec->descriptor_count; ++k) {
            const auto d = c->Descriptor(*rec, k);
            if (!d) continue;
            const auto piece =
                decode_ndxr_descriptor(*c, bytes.data(), bytes.size(), *d);
            if (!piece) continue;
            for (const auto& p : piece->positions) all.emplace_back(p.x, p.z);
          }
        }
      }
    }
    if (!all.empty()) {
      const std::size_t stride = all.size() / kMaxPoints + 1;
      for (std::size_t i = 0; i < all.size(); i += stride) f.points.push_back(all[i]);
      f.valid = true;
    }
    return parts.emplace(id, f).first->second;
  };

  struct Score { std::size_t wet = 0, seen = 0; };
  Score s_none, s_plus, s_minus, s_null;
  // The coastal subset: instances whose footprint touches water under ANY of the
  // arrangements. Inland buildings carry no signal about rotation at all, and
  // averaging them in is what made the first version undecidable.
  Score c_none, c_plus, c_minus, c_null;
  std::size_t coastal = 0;
  std::uint32_t seed = 987654321u;
  const float kPi = 3.14159265358979F;

  for (const MapInstance& q : placement->instances()) {
    const Footprint& f = footprint(q.part_id);
    if (!f.valid) continue;
    const float theta = 2.0F * kPi * static_cast<float>(q.tag_high) / 65536.0F;
    const float random_theta =
        2.0F * kPi * static_cast<float>(next(seed) % 65536u) / 65536.0F;
    const float angles[4] = {0.0F, theta, -theta, random_theta};
    Score* global[4] = {&s_none, &s_plus, &s_minus, &s_null};
    Score* coast[4] = {&c_none, &c_plus, &c_minus, &c_null};
    Score local[4];
    for (int a = 0; a < 4; ++a) {
      const float ct = std::cos(angles[a]), st = std::sin(angles[a]);
      for (const auto& pt : f.points) {
        const float wx = q.world_x + pt.first * ct - pt.second * st;
        const float wz = q.world_z + pt.first * st + pt.second * ct;
        bool bit = false;
        if (!water->query(wx, wz, &bit)) continue;
        ++local[a].seen;
        if (bit) ++local[a].wet;
      }
    }
    bool touches = false;
    for (int a = 0; a < 4; ++a) touches = touches || local[a].wet > 0;
    if (touches) ++coastal;
    for (int a = 0; a < 4; ++a) {
      global[a]->seen += local[a].seen; global[a]->wet += local[a].wet;
      if (touches) { coast[a]->seen += local[a].seen; coast[a]->wet += local[a].wet; }
    }
  }
  auto show = [](const char* label, const Score& s) {
    std::printf("  %-22s %8zu of %8zu footprint samples over water  %6.3f%%\n",
                label, s.wet, s.seen, 100.0 * s.wet / s.seen);
  };
  std::printf("parts measured %zu, up to %zu real vertices each; %zu coastal instances\n",
              parts.size(), kMaxPoints, coastal);
  std::printf("all instances:\n");
  show("no rotation", s_none);
  show("+theta", s_plus);
  show("-theta", s_minus);
  show("random angle (null)", s_null);
  std::printf("coastal instances only:\n");
  show("no rotation", c_none);
  show("+theta", c_plus);
  show("-theta", c_minus);
  show("random angle (null)", c_null);
  s_none = c_none; s_plus = c_plus; s_minus = c_minus; s_null = c_null;

  const double plus = 100.0 * s_plus.wet / s_plus.seen;
  const double minus = 100.0 * s_minus.wet / s_minus.seen;
  const double null = 100.0 * s_null.wet / s_null.seen;
  const double none = 100.0 * s_none.wet / s_none.seen;
  std::printf("\n");
  if (plus > null && minus > null) {
    std::printf("UNDECIDED: neither sign beats the null, so the footprint proxy\n"
                "is too blunt to choose. Reporting that rather than a winner.\n");
    return 0;
  }
  const bool plus_wins = plus < minus;
  const double gap = std::fabs(plus - minus);
  std::printf("%s is drier by %.3f points; the null is %.3f and no rotation %.3f.\n",
              plus_wins ? "+theta" : "-theta", gap, null, none);
  if (gap < 0.05) {
    std::printf("The gap is inside the noise this proxy can resolve -- UNDECIDED.\n");
  }
  return 0;
}
